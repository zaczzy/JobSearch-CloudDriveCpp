#ifndef A5A63C27_F54B_4BF3_A57D_082073F816F1
#define A5A63C27_F54B_4BF3_A57D_082073F816F1
#include <cstddef>  // std::size_t
#include <string>
#include <vector>
#include "backendrelay.h"
#define MAX_CHUNK_PLUS_SIZE 10240
void request_file_names(std::vector<std::string>& filenames,
                        const std::string& path, const std::string& username,
                        const std::string& folder_name, BackendRelay* BR);
bool request_file_download(const std::string& username,
                           const std::string& current_path,
                           const std::string& fname, BackendRelay* BR);
void download_file_chunk(std::string& chunk, bool* read_ready, size_t* f_len,
                         BackendRelay* BR);
std::string replace_first_occurrence(std::string& s,
                                     const std::string& toReplace,
                                     const std::string& replaceWith);
std::string replace_all_occurrences(std::string& s,
                                    const std::string& toReplace,
                                    const std::string& replaceWith);
void generate_display_list(std::string& to_replace,
                           const std::string& current_path,
                           const std::vector<std::string>& filenames);
void split_filename(const std::string& str, std::string& path,
                    std::string& folder);
bool upload_next_part(int* total_body_read, std::string& filename, int sock,
                      std::string& boundary, std::string& username,
                      std::string& directory_path, BackendRelay * BR);
#endif /* A5A63C27_F54B_4BF3_A57D_082073F816F1 */
