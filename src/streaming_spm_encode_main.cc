// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include <functional>
#include <string>
#include <vector>
#include <iostream>

#include "common.h"
#include "filesystem.h"
#include "init.h"
#include "sentencepiece.pb.h"
#include "sentencepiece_processor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "trainer_interface.h"
#include "util.h"

ABSL_FLAG(std::string, model, "", "model file name");
ABSL_FLAG(
    std::string, output_format, "piece",
    "choose from piece, id, bid, proto, nbest_piece, nbest_id, or nbest_proto");
ABSL_FLAG(std::string, extra_options, "",
          "':' separated encoder extra options, e.g., \"reverse:bos:eos\"");
ABSL_FLAG(int32, nbest_size, 10, "NBest size");
ABSL_FLAG(double, alpha, 0.5, "Smoothing parameter for sampling mode.");
ABSL_FLAG(uint32, random_seed, ~0u,
          "Seed value for random generator.");
ABSL_FLAG(int, new_line_delim, '\n', "Sentence delimiter char.");


// Piece restriction with vocabulary file.
// https://github.com/rsennrich/subword-nmt#best-practice-advice-for-byte-pair-encoding-in-nmt
ABSL_FLAG(std::string, vocabulary, "",
          "Restrict the vocabulary. The encoder only emits the "
          "tokens in \"vocabulary\" file");
ABSL_FLAG(int32, vocabulary_threshold, 0,
          "Words with frequency < threshold will be treated as OOV");
ABSL_FLAG(bool, generate_vocabulary, false,
          "Generates vocabulary file instead of segmentation");

int main(int argc, char *argv[]) {
  sentencepiece::ScopedResourceDestructor cleaner;
  sentencepiece::ParseCommandLineFlags(argv[0], &argc, &argv, true);
  std::vector<std::string> rest_args;

  for (int i = 1; i < argc; ++i) {
    rest_args.emplace_back(argv[i]);
  }

  if (absl::GetFlag(FLAGS_random_seed) != ~0u) {
    sentencepiece::SetRandomGeneratorSeed(absl::GetFlag(FLAGS_random_seed));
  }

  if (rest_args.empty())
    rest_args.push_back("");  // empty means that reading from stdin.

  CHECK(!absl::GetFlag(FLAGS_model).empty());

  sentencepiece::SentencePieceProcessor sp;
  CHECK_OK(sp.Load(absl::GetFlag(FLAGS_model)));
  CHECK_OK(sp.SetEncodeExtraOptions(absl::GetFlag(FLAGS_extra_options)));

  if (!absl::GetFlag(FLAGS_vocabulary).empty()) {
    CHECK_OK(sp.LoadVocabulary(absl::GetFlag(FLAGS_vocabulary),
                               absl::GetFlag(FLAGS_vocabulary_threshold)));
  }

  auto output = sentencepiece::filesystem::NewWritableFile("");
  CHECK_OK(output->status());
  absl::flat_hash_map<std::string, int> vocab;
  sentencepiece::SentencePieceText spt;
  sentencepiece::NBestSentencePieceText nbest_spt;
  std::function<void(absl::string_view line)> process;
  std::vector<uint32_t> sentence_sizes;
  int eos = sp.eos_id(), bos = sp.bos_id();
  char verbatim_control_char = sp.model_proto()->trainer_spec().verbatim_control_char();

  const int nbest_size = absl::GetFlag(FLAGS_nbest_size);
  const float alpha = absl::GetFlag(FLAGS_alpha);

  if (absl::GetFlag(FLAGS_generate_vocabulary)) {
    process = [&](absl::string_view line) {
      sentencepiece::SentencePieceText spt;
      CHECK_OK(sp.Encode(line, &spt));
      for (const auto &piece : spt.pieces()) {
        if (!sp.IsUnknown(piece.id()) && !sp.IsControl(piece.id()))
          vocab[piece.piece()]++;
      }
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "piece") {
    process = [&](absl::string_view line) {
      std::vector<std::string> sps;
      CHECK_OK(sp.Encode(line, &sps));
      output->WriteLine(absl::StrJoin(sps, " "));
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "id") {
    process = [&](absl::string_view line) {
      std::vector<int> ids;
      CHECK_OK(sp.Encode(line, &ids));
      output->WriteLine(absl::StrJoin(ids, " "));
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "bid") {
    process = [&](absl::string_view line) {
      std::vector<int> ids;
      CHECK_OK(sp.Encode(line, &ids));
      if (!line.empty() && line[0] == verbatim_control_char) {
        ids.insert(ids.begin(), bos);
      }
      output->Write(absl::string_view(
          reinterpret_cast<char *>(ids.data()), sizeof(int) * ids.size()));
      output->Write(absl::string_view(
          reinterpret_cast<char *>(&eos), sizeof(int)));
      sentence_sizes.push_back(ids.size());
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "proto") {
    process = [&](absl::string_view line) { CHECK_OK(sp.Encode(line, &spt)); };
  } else if (absl::GetFlag(FLAGS_output_format) == "sample_piece") {
    process = [&](absl::string_view line) {
      std::vector<std::string> sps;
      CHECK_OK(sp.SampleEncode(line, nbest_size, alpha, &sps));
      output->WriteLine(absl::StrJoin(sps, " "));
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "sample_id") {
    process = [&](absl::string_view line) {
      std::vector<int> ids;
      CHECK_OK(sp.SampleEncode(line, nbest_size, alpha, &ids));
      output->WriteLine(absl::StrJoin(ids, " "));
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "sample_proto") {
    process = [&](absl::string_view line) {
      CHECK_OK(sp.SampleEncode(line, nbest_size, alpha, &spt));
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "nbest_piece") {
    process = [&](absl::string_view line) {
      std::vector<std::vector<std::string>> nbest_sps;
      CHECK_OK(sp.NBestEncode(line, nbest_size, &nbest_sps));
      for (const auto &result : nbest_sps) {
        output->WriteLine(absl::StrJoin(result, " "));
      }
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "nbest_id") {
    process = [&](absl::string_view line) {
      std::vector<std::vector<int>> nbest_ids;
      CHECK_OK(sp.NBestEncode(line, nbest_size, &nbest_ids));
      for (const auto &result : nbest_ids) {
        output->WriteLine(absl::StrJoin(result, " "));
      }
    };
  } else if (absl::GetFlag(FLAGS_output_format) == "nbest_proto") {
    process = [&](absl::string_view line) {
      CHECK_OK(sp.NBestEncode(line, nbest_size, &nbest_spt));
    };
  } else {
    LOG(FATAL) << "Unknown output format: "
               << absl::GetFlag(FLAGS_output_format);
  }

  std::string line;
  char delim = absl::GetFlag(FLAGS_new_line_delim);
  while (std::getline(std::cin, line, delim)) {
    process(line);
  }

  if (absl::GetFlag(FLAGS_output_format) == "bid") {
    size_t count = sentence_sizes.size();
    output->Write(absl::string_view(reinterpret_cast<char *>(sentence_sizes.data()),
                  sizeof(sentence_sizes[0]) * sentence_sizes.size()));
    output->Write(absl::string_view(reinterpret_cast<char *>(&count), sizeof(count)));
  }

  return 0;
}