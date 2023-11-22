#include "mixed_text_code_handler.h"
#include <fstream>

namespace sentencepiece {

bool MixedTextCodeIterator::HasCodeHeader() const {
  return code_meta_block_begin_ >= 0 && head_ == cache_value_.data() && head_ != tail_ && *head_ == code_meta_block_begin_;
}

std::optional<MixedTextCodeIterator::BlockType> MixedTextCodeIterator::ReadCodeHeader(absl::string_view* line) {
  if (*head_ != code_meta_block_begin_) {
    LOG(WARNING) << "Error: Assertion failed - head_ != code_meta_block_begin_";
    return BlockType::Error;
  }
  if (code_meta_block_end_ < 0) {
    LOG(WARNING) << "Error: Assertion failed - code_meta_block_end_ < 0";
    return BlockType::Error;
  }
  auto ptr = reinterpret_cast<const char *>(memchr(head_, code_meta_block_end_, tail_ - head_));
  if (ptr != nullptr) {
    *line = absl::string_view(head_ + 1, ptr - head_ - 1);
    head_ = ptr + 1;
    return BlockType::CodeHeader;
  } else {
    LOG(WARNING) << "Code meta block did not end with code meta block end character";
    return BlockType::Error;
  }
}

std::optional<MixedTextCodeIterator::BlockType> MixedTextCodeIterator::ReadTextBlock(absl::string_view* line) {
  const char* ptr;
  if (verbatim_control_char_ >= 0) {
    ptr = reinterpret_cast<const char *>(memchr(head_, verbatim_control_char_, tail_ - head_));
  } else {
    ptr = nullptr;
  }
  if (ptr == nullptr) {
    *line = absl::string_view(head_, tail_ - head_);
    head_ = tail_;
  } else {
    *line = absl::string_view(head_, ptr - head_);
    head_ = ptr;
    in_text_ = false;
  }
  return line->size() > 0 ? std::make_optional(BlockType::Text) : std::nullopt;
}

std::optional<MixedTextCodeIterator::BlockType> MixedTextCodeIterator::ReadCodeBlock(absl::string_view* line) {
  if (*head_ != verbatim_control_char_) {
    LOG(WARNING) << "Error: Assertion failed - head_ != verbatim_control_char_";
    return BlockType::Error;
  }
  auto ptr = reinterpret_cast<const char *>(memchr(head_, code_block_end_, tail_ - head_));
  if (ptr == nullptr) {
    LOG(WARNING) << "Code block does not end with code block end character";
    return BlockType::Error;
  }
  *line = absl::string_view(head_, ptr - head_);
  head_ = ptr + 1;
  in_text_ = true;
  return line->size() > 1 ? std::make_optional(BlockType::Code) : std::nullopt; // skip x01
}

std::optional<MixedTextCodeIterator::BlockType> MixedTextCodeIterator::TryReadNext(absl::string_view* line) {
  if (HasCodeHeader()) {
    return ReadCodeHeader(line);
  } else if (in_text_) {
    // Expecting text block
    return ReadTextBlock(line);
  } else {
    // Expecting code block
    return ReadCodeBlock(line);
  }
}


MixedTextCodeIterator::MixedTextCodeIterator(absl::string_view cache_value,
  int32 verbatim_control_char,
  int32 code_block_end,
  int32 code_meta_block_begin,
  int32 code_meta_block_end
):
cache_value_(cache_value),
in_text_(true),
head_(cache_value_.data()),
tail_(cache_value_.data() + cache_value.size()),
verbatim_control_char_(verbatim_control_char),
code_block_end_(code_block_end),
code_meta_block_begin_(code_meta_block_begin),
code_meta_block_end_(code_meta_block_end)
{
}

int MixedTextCodeIterator::_total_files = 0;
int MixedTextCodeIterator::_total_error_files = 0;
std::mutex MixedTextCodeIterator::_mutex_error;

std::optional<MixedTextCodeIterator::BlockType> MixedTextCodeIterator::Next(absl::string_view* line) {
  while (HasNext()) {
    // Skips empty blocks
    if (auto r = TryReadNext(line); r.has_value()) {
      if (r.value() == BlockType::Error) {
        std::lock_guard<std::mutex> _lock(_mutex_error);
        // Append the content of line to a local file
        std::ofstream ofs("mixed_text_code_handler_error.txt", std::ios_base::app);
        ofs << *line << std::endl;
        ofs.close();
        _total_error_files++;
        LOG(WARNING) << "Total error files: " << _total_error_files << " out of " << _total_files << " files";
        return std::nullopt;
      } else {
        _total_files++;
      }
      return r;
    }
  }
  return std::nullopt;
}

bool MixedTextCodeIterator::HasNext() const {
  return head_ < tail_;
}

}  // namespace sentencepiece
