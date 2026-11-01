/*
	-= 3-LANGUAGE-MODEL (3LM) by nullz =-

	Compile & run with:
	gcc ./3LM.c -lm -O3 -o ./3LM && ./3LM ./wiki/wiki.txt

	# HOW IT WORKS

	Very simple. It requires zero training time, doesnt require any vram, and works purely on the CPU.
	I was inspired by the magical ReLU&temperature function.
	It could turn anything with some "score" into a list of chances, with the distribution being able to be tweaked with the temperature.
	It is widely used for language models, where you have to turn a list of "scores" into chances, where they must sum up to exactly 1.
	
	Now, these "chances" make up a complete language model (even if it is stupid, it is considered a language model).
	So all we need for a LM - is to get some list of tokens with scores, based on some input.

	There are a lot of possibilities for that, and i have thought of this:
	We take 3 characters of the input, then look up for any matches with the dataset text.
	If we find the match, add the next token of the dataset text to the tokens list.
	The amount of same tokens appearing after the match in the dataset is "score".

	Then, we can make up a list of chances using the ReLU function, and tada, we have the language model!
	Although, it is gonna get slightly more complex and capable over the development time.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define DATASET_BUFFER_SIZE 100000
#define INPUT_BUFFER_SIZE 1000
#define TOKENS_COUNT 50
#define PRECISION_SHIFT 2.0
#define TEMPERATURE 10.0

struct token {
	char character;
	int score;
};

struct output {
	char character;
	double chance;
};

int getText(char* buffer, FILE *f, size_t* size) {
	if (f == NULL) {
		printf("Failed to open file!\nDoes it exist?");
		return 1;
	}

	size_t n = fread(buffer, 1, DATASET_BUFFER_SIZE - 1, f);
	buffer[n] = '\0';
	*size = n;

	return 0;
}

void nextTokens(size_t n, char* input, int inIdx, char* buffer, struct token tokens[], int* tokIdx) {
	for (int i=0; i<TOKENS_COUNT; i++) {
		tokens[i].character = '\0';
		tokens[i].score = 0;
	}
	*tokIdx = 0;
	for (size_t i=0; i+2<n; i++) {
		if (*tokIdx == TOKENS_COUNT) break;
		if (input[inIdx] == buffer[i] && input[inIdx+1] == buffer[i+1] && input[inIdx+2] == buffer[i+2]) {
			int tokenExists = 0;
			for (int j=0; j<TOKENS_COUNT; j++) {
				if (tokens[j].character == buffer[i+3]) {
					tokens[j].score++;
					tokenExists = 1;
	
					int doesMatch = 1;
					int matchCount = 1;
					int checkIdx = inIdx;
					while (doesMatch == 1) {
						checkIdx--;
						if (checkIdx < 0) break;
						if (input[checkIdx] == buffer[i-matchCount]) {
							doesMatch = 1;
							matchCount++;
							tokens[j].score++;
						} else doesMatch = 0;
					}
				}
			}
			if (tokenExists == 0) {
				tokens[*tokIdx].character = buffer[i+3];
				tokens[*tokIdx].score++;
				(*tokIdx)++;

				int doesMatch = 1;
				int matchCount = 1;
				int checkIdx = inIdx;
				while (doesMatch == 1) {
					checkIdx--;
					if (checkIdx < 0) break;
					if (input[checkIdx] == buffer[i-matchCount]) {
						doesMatch = 1;
						matchCount++;
						tokens[*tokIdx].score++;
					} else doesMatch = 0;
				}
			}
		}
	}
}

void ReLU(struct token tokens[], int tokIdx, double temp, struct output outputs[]) {
	if (temp == 0.0) temp = 1e-3;
	for (int i=0; i<TOKENS_COUNT; i++) {
		outputs[i].character = tokens[i].character;
		outputs[i].chance = 0.0;
	}

	int tokCount = tokIdx - 1;
	double expos[TOKENS_COUNT];
	double expoSum = 0.0;
	int biggestScore = 0;
	for (int i=0; i<tokIdx; i++) {
		if (tokens[i].score > biggestScore) biggestScore = tokens[i].score;
		expos[i] = 0.0;
	}
	for (int i=0; i<tokIdx; i++) {
		expos[i] = exp((tokens[i].score-biggestScore+PRECISION_SHIFT)/temp);
		expoSum += expos[i];
	}
	for (int i=0; i<tokIdx; i++) {
		outputs[i].chance = expos[i]/expoSum;
	}
}

int main(int argc, char *argv[]) {
	clock_t start = clock();

	FILE *f = fopen(argv[1], "r");
	char buffer[DATASET_BUFFER_SIZE];
	size_t n = 0;
	if (getText(buffer, f, &n) != 0) return 1;
	//printf(buffer);
	
	clock_t afterText = clock();
	double msec = (afterText-start)*1000.0/CLOCKS_PER_SEC;
	printf("Opened and read %s in %0.5f ms\n", argv[1], msec);

	int inIdx = -4;
	char input[INPUT_BUFFER_SIZE];
	if (fgets(input, sizeof(input), stdin) == NULL) return 1;
	for (int i=0; i<INPUT_BUFFER_SIZE; i++) {
		if (input[i] != '\0') inIdx++;
		else break;
	}

	start = clock();

	struct token tokens[TOKENS_COUNT];
	struct output outputs[TOKENS_COUNT];
	int tokIdx = 0;
	nextTokens(n, input, inIdx, buffer, tokens, &tokIdx);
	ReLU(tokens, tokIdx, TEMPERATURE, outputs);

	double totalChance = 0.0;
	for (int i=0; i<tokIdx; i++) {
		totalChance += outputs[i].chance;
	}

	clock_t afterCompute = clock();
	msec = (afterCompute-start)*1000.0/CLOCKS_PER_SEC;

	printf("GUESSES:\n");
	for (int i=0; i<tokIdx; i++) {
		printf("CHAR: '%c' | SCORE: %d | CHANCE: %.10lf\n", tokens[i].character, tokens[i].score, outputs[i].chance);
	}
	printf("CHANCES SUM: %0.10lf (Off by %0.10lf)\n", totalChance, totalChance-1.0);
	printf("COMPUTE TIME: %0.5lf ms", msec);
}
