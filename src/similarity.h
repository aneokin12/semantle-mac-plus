#ifndef SEMANTLE_PLUS_SIMILARITY_H
#define SEMANTLE_PLUS_SIMILARITY_H

/*
 * The generated model is a compact, signed-byte word2vec table.  Vectors
 * are normalized and quantized offline, so the 68000 only performs integer
 * dot products at play time.
 */
#define EMBEDDING_DIMENSION 50
#define MAX_WORD_LENGTH 23
#define MAX_CANDIDATE_WORDS 4096
#define TOP_NEIGHBOR_COUNT 50

typedef struct {
    short raw_score;
    short rank_score;
    short indexed;
} SimilarityResult;

void word_bank_init(void);
unsigned short candidate_word_count(void);
const char *candidate_word_at(unsigned short index);
short candidate_index_for_word(const char *word);

/* Prepare the target's ranked top-neighbor list for the current round. */
void prepare_top_neighbors(unsigned short target_index,
                           unsigned short top_indices[TOP_NEIGHBOR_COUNT]);

/* Score against an indexed target, using the prepared top-neighbor index. */
SimilarityResult similarity_for_target(const char *guess,
                                       unsigned short target_index,
                                       const unsigned short top_indices[TOP_NEIGHBOR_COUNT]);

/* Returns the inclusive 0..100 raw cosine-like score for any ASCII words. */
short similarity_score(const char *left, const char *right);

#endif
