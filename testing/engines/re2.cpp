/**
 * This is a wrapper around some RE2 code to provide a standard C calling interface.
 * 
 * I am really not a C++ expert, so I have no idea if these things are efficient or not,
 * but I think the long and the short of it is that a C++ libary is probably not very
 * embedded compatible anyway!
 */

#include <re2/re2.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>


// ---- internal state ----
static RE2 *re2_regex = nullptr;
static std::string re2_subject;                       // last matched subject
static std::vector<re2::StringPiece> re2_subs;        // last captured submatches
static int re2_ngroups = 0;                           // number of capturing groups

static int re2_compile(char *pattern_c) {
    std::string pattern(pattern_c);

    re2_regex = new RE2(pattern, RE2::DefaultOptions);
    if (!re2_regex->ok()) {
        // compilation failed
        delete re2_regex;
        re2_regex = nullptr;
        return 0;
    }

    // number of capturing groups (this includes our wrapper outer group)
    re2_ngroups = re2_regex->NumberOfCapturingGroups();
    if (re2_ngroups < 0) re2_ngroups = 0;

    // prepare storage for capture pieces
    re2_subs.resize(re2_ngroups); // subs[0] corresponds to the outer wrapper (full match)
    return 1;
}

static int re2_match(char *text) {
    re2_subject = std::string(text);
    // re2::StringPiece needs to point to re2_subject data:
    re2::StringPiece subject_sp(re2_subject);

    re2_ngroups++;

    // prepare a temporary vector (will be filled by RE2::Match)
    std::vector<re2::StringPiece> tmp;
    tmp.resize(re2_ngroups);

    if (! re2_regex->Match(subject_sp, 0, re2_subject.size(),
                               RE2::UNANCHORED, tmp.data(), static_cast<int>(tmp.size()))) {
        // no match
        re2_subs.clear();
        return 0;
    }       
    // copy tmp into persistent re2_subs
    re2_subs = tmp;
    return 1;
}

// number of results (mimics libc behaviour: re_nsub + 1)
static int re2_res_count() {
    // re2_ngroups already includes the wrapped full-match group,
    // which equals (original_re_nsub + 1). This maps directly to the POSIX count.
    return re2_ngroups;
}

// start offset of group `res` (POSIX-style: 0=full match, 1..n = subgroups)
static int re2_res_so(int res) {
    if (re2_subs.empty()) return -1;
    if (res < 0 || res >= (int)re2_subs.size()) return -1;
    re2::StringPiece &sp = re2_subs[res];
    if (sp.data() == nullptr) return -1;
    ptrdiff_t off = sp.data() - re2_subject.data();
    return (int)off;
}

// end offset (one past last character) of group `res`
static int re2_res_eo(int res) {
    if (re2_subs.empty()) return -1;
    if (res < 0 || res >= (int)re2_subs.size()) return -1;
    re2::StringPiece &sp = re2_subs[res];
    if (sp.data() == nullptr) return -1;
    ptrdiff_t off = sp.data() - re2_subject.data();
    return (int)(off + sp.size());
}

static int re2_free() {
    delete re2_regex;
    re2_regex = nullptr;
    re2_subject.clear();
    re2_subs.clear();
    re2_ngroups = 0;
    return 1;
}

extern "C" {

    #include "../test.h"

    struct engine funcs_re2 = {
        .name = (char *)"re2",
        .compile = re2_compile,
        .match = re2_match,
        .res_count = re2_res_count,
        .res_so = re2_res_so,
        .res_eo = re2_res_eo,
        .free = re2_free,
    };
}