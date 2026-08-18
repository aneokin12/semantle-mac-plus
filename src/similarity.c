#include "similarity.h"

#include "model_data.h"

#define QUANTIZED_LIMIT 127L
/* Normalized vectors have a maximum dot product of 127^2, not 50*127^2. */
#define MAX_DOT_PRODUCT (QUANTIZED_LIMIT * QUANTIZED_LIMIT)

static unsigned char lower_ascii(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z')
        return (unsigned char)(c + ((unsigned char)'a' - (unsigned char)'A'));
    return c;
}

static short compare_word(const char *left, const char *right)
{
    unsigned short i = 0;
    unsigned char left_char;
    unsigned char right_char;

    if (left == 0)
        return right == 0 ? 0 : -1;
    if (right == 0)
        return 1;
    while (left != 0 && right != 0) {
        left_char = lower_ascii((unsigned char)left[i]);
        right_char = lower_ascii((unsigned char)right[i]);
        if (left_char < right_char)
            return -1;
        if (left_char > right_char)
            return 1;
        if (left[i] == 0 || right[i] == 0)
            break;
        ++i;
    }
    if (left[i] == 0 && right[i] == 0)
        return 0;
    return left[i] == 0 ? -1 : 1;
}

static short find_word(const char *word)
{
    short low = 0;
    short high = (short)MODEL_WORD_COUNT - 1;
    short middle;
    short comparison;

    if (word == 0 || word[0] == 0)
        return -1;
    while (low <= high) {
        middle = (short)((low + high) / 2);
        comparison = compare_word(word, kModelWords[middle]);
        if (comparison == 0)
            return middle;
        if (comparison < 0)
            high = (short)(middle - 1);
        else
            low = (short)(middle + 1);
    }
    return -1;
}

static long vector_dot(const signed char *left, const signed char *right)
{
    short i;
    long result = 0;

    for (i = 0; i < EMBEDDING_DIMENSION; ++i)
        result += (long)left[i] * (long)right[i];
    return result;
}

static unsigned long integer_sqrt(unsigned long value)
{
    unsigned long low = 0;
    unsigned long high = 65536UL;
    unsigned long middle;

    while (low + 1UL < high) {
        middle = (low + high) / 2UL;
        if (middle <= value / middle)
            low = middle;
        else
            high = middle;
    }
    return low;
}

static unsigned short feature_bucket(unsigned long hash)
{
    return (unsigned short)(hash % (unsigned long)EMBEDDING_DIMENSION);
}

static void add_fallback_feature(long sums[EMBEDDING_DIMENSION],
                                 unsigned long hash)
{
    unsigned short bucket = feature_bucket(hash);
    long magnitude = 16L + (long)((hash >> 5) & 15UL);

    if ((hash & 1UL) != 0)
        sums[bucket] += magnitude;
    else
        sums[bucket] -= magnitude;
}

/*
 * OOV words use a deterministic character n-gram vector.  It is deliberately
 * only a fallback: indexed word2vec vectors receive the game's calibrated
 * top-50 rank treatment.
 */
static void fallback_vector(const char *word, signed char output[EMBEDDING_DIMENSION])
{
    long sums[EMBEDDING_DIMENSION];
    unsigned short i;
    unsigned long hash;
    unsigned char first;
    unsigned char second;
    unsigned long norm;

    for (i = 0; i < EMBEDDING_DIMENSION; ++i)
        sums[i] = 0;
    if (word == 0)
        word = "";

    i = 0;
    while (word[i] != 0 && i < MAX_WORD_LENGTH) {
        first = lower_ascii((unsigned char)word[i]);
        hash = 2166136261UL ^ (unsigned long)first;
        hash *= 16777619UL;
        hash ^= (unsigned long)i + 31UL;
        add_fallback_feature(sums, hash);

        if (word[i + 1] != 0 && i + 1 < MAX_WORD_LENGTH) {
            second = lower_ascii((unsigned char)word[i + 1]);
            hash ^= ((unsigned long)second << 8) | (unsigned long)first;
            hash *= 16777619UL;
            add_fallback_feature(sums, hash);
        }
        ++i;
    }

    norm = 0;
    for (i = 0; i < EMBEDDING_DIMENSION; ++i)
        norm += (unsigned long)(sums[i] * sums[i]);
    norm = integer_sqrt(norm);
    if (norm == 0)
        norm = 1;
    for (i = 0; i < EMBEDDING_DIMENSION; ++i) {
        long scaled = sums[i] * QUANTIZED_LIMIT / (long)norm;
        if (scaled > QUANTIZED_LIMIT)
            scaled = QUANTIZED_LIMIT;
        if (scaled < -QUANTIZED_LIMIT)
            scaled = -QUANTIZED_LIMIT;
        output[i] = (signed char)scaled;
    }
}

static short score_from_dot(long dot)
{
    long score;

    score = (dot + MAX_DOT_PRODUCT) * 100L / (2L * MAX_DOT_PRODUCT);
    if (score < 0)
        score = 0;
    if (score > 100)
        score = 100;
    return (short)score;
}

static short rank_for_index(unsigned short index,
                            const unsigned short top_indices[TOP_NEIGHBOR_COUNT])
{
    short i;

    for (i = 0; i < TOP_NEIGHBOR_COUNT; ++i) {
        if (top_indices[i] == index)
            return (short)(TOP_NEIGHBOR_COUNT - i);
    }
    return 0;
}

void word_bank_init(void)
{
    /* The generated table is read-only and needs no runtime initialization. */
}

unsigned short candidate_word_count(void)
{
    return (unsigned short)MODEL_WORD_COUNT;
}

const char *candidate_word_at(unsigned short index)
{
    if (index >= MODEL_WORD_COUNT)
        return 0;
    return kModelWords[index];
}

short candidate_index_for_word(const char *word)
{
    return find_word(word);
}

void prepare_top_neighbors(unsigned short target_index,
                           unsigned short top_indices[TOP_NEIGHBOR_COUNT])
{
    short i;
    short j;
    short position;
    long target_dot;
    long top_dots[TOP_NEIGHBOR_COUNT];
    const signed char *target_vector;

    for (i = 0; i < TOP_NEIGHBOR_COUNT; ++i) {
        top_indices[i] = (unsigned short)MODEL_WORD_COUNT;
        top_dots[i] = 0;
    }
    if (target_index >= MODEL_WORD_COUNT)
        return;

    target_vector = kModelVectors[target_index];
    for (i = 0; i < MODEL_WORD_COUNT; ++i) {
        target_dot = vector_dot(target_vector, kModelVectors[i]);
        position = 0;
        while (position < TOP_NEIGHBOR_COUNT &&
               top_indices[position] < MODEL_WORD_COUNT) {
            if (target_dot > top_dots[position] ||
                (target_dot == top_dots[position] && i < top_indices[position]))
                break;
            ++position;
        }
        if (position < TOP_NEIGHBOR_COUNT) {
            for (j = TOP_NEIGHBOR_COUNT - 1; j > position; --j) {
                top_indices[j] = top_indices[j - 1];
                top_dots[j] = top_dots[j - 1];
            }
            top_indices[position] = (unsigned short)i;
            top_dots[position] = target_dot;
        }
    }
}

SimilarityResult similarity_for_target(const char *guess,
                                       unsigned short target_index,
                                       const unsigned short top_indices[TOP_NEIGHBOR_COUNT])
{
    SimilarityResult result;
    signed char fallback[EMBEDDING_DIMENSION];
    const signed char *guess_vector;
    short guess_index;
    long dot;

    result.raw_score = 0;
    result.rank_score = 0;
    result.indexed = 0;
    if (target_index >= MODEL_WORD_COUNT)
        return result;

    guess_index = find_word(guess);
    if (guess_index >= 0) {
        guess_vector = kModelVectors[guess_index];
        result.indexed = 1;
        if (guess_index == (short)target_index)
            result.rank_score = TOP_NEIGHBOR_COUNT;
        else
            result.rank_score = rank_for_index((unsigned short)guess_index,
                                               top_indices);
    } else {
        fallback_vector(guess, fallback);
        guess_vector = fallback;
    }
    dot = vector_dot(guess_vector, kModelVectors[target_index]);
    result.raw_score = score_from_dot(dot);
    return result;
}

short similarity_score(const char *left, const char *right)
{
    signed char left_fallback[EMBEDDING_DIMENSION];
    signed char right_fallback[EMBEDDING_DIMENSION];
    const signed char *left_vector;
    const signed char *right_vector;
    short left_index;
    short right_index;

    if (compare_word(left, right) == 0)
        return 100;
    left_index = find_word(left);
    right_index = find_word(right);
    if (left_index >= 0)
        left_vector = kModelVectors[left_index];
    else {
        fallback_vector(left, left_fallback);
        left_vector = left_fallback;
    }
    if (right_index >= 0)
        right_vector = kModelVectors[right_index];
    else {
        fallback_vector(right, right_fallback);
        right_vector = right_fallback;
    }
    return score_from_dot(vector_dot(left_vector, right_vector));
}
