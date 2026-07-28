#include "documents.hpp"

namespace ebasic::lsp {

void DocumentStore::open(const std::string& uri, std::string text) {
    documents_[uri] = std::move(text);
}

void DocumentStore::update(const std::string& uri, std::string text) {
    documents_[uri] = std::move(text);
}

void DocumentStore::close(const std::string& uri) {
    documents_.erase(uri);
}

const std::string* DocumentStore::find(const std::string& uri) const {
    auto it = documents_.find(uri);
    return it == documents_.end() ? nullptr : &it->second;
}

} // namespace ebasic::lsp
