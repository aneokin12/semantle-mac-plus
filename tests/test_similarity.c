#include "similarity.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    unsigned short i;
    unsigned short top[TOP_NEIGHBOR_COUNT];
    SimilarityResult result;
    short target;

    word_bank_init();
    assert(candidate_word_count() >= 1000);
    assert(similarity_score("cat", "cat") == 100);
    assert(similarity_score("CAT", "cat") == 100);
    assert(similarity_score("cat", "dog") > similarity_score("cat", "hammer"));
    assert(similarity_score("piano", "guitar") > similarity_score("piano", "potato"));
    assert(similarity_score("completely", "unknown") >= 0);
    assert(similarity_score("completely", "unknown") <= 100);
    assert(candidate_index_for_word("CAT") == candidate_index_for_word("cat"));
    assert(candidate_index_for_word("word-that-is-not-indexed") < 0);

    target = candidate_index_for_word(candidate_word_at(0));
    assert(target == 0);
    prepare_top_neighbors((unsigned short)target, top);
    result = similarity_for_target(candidate_word_at(0), (unsigned short)target, top);
    assert(result.indexed != 0);
    assert(result.rank_score == TOP_NEIGHBOR_COUNT);
    result = similarity_for_target("word-that-is-not-indexed",
                                   (unsigned short)target, top);
    assert(result.indexed == 0);
    assert(result.rank_score == 0);
    assert(result.raw_score >= 0 && result.raw_score <= 100);
    result = similarity_for_target(candidate_word_at(top[TOP_NEIGHBOR_COUNT - 1]),
                                   (unsigned short)target, top);
    assert(result.rank_score == 1);

    for (i = 0; i < candidate_word_count(); ++i) {
        assert(candidate_word_at(i) != 0);
        assert(similarity_score(candidate_word_at(i), candidate_word_at(i)) == 100);
    }

    printf("%u candidate words; similarity checks passed\n", candidate_word_count());
    return 0;
}
