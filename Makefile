.PHONY: test classic

test:
	clang -std=c89 -Wall -Wextra -Wpedantic -Isrc src/similarity.c tests/test_similarity.c -o /private/tmp/semantle-plus-test
	/private/tmp/semantle-plus-test

classic:
	sh ./build.sh
