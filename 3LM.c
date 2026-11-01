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

#define DATASET_BUFFER_SIZE 5250000
#define INPUT_BUFFER_SIZE 200
#define TOKENS_COUNT 150
#define PRECISION_SHIFT 2.0
#define PREV_CHAR_MATCH_GRANT_MUL 2.0
#define TEMPERATURE 0.05

struct token {
	char character;
	double score;
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
		tokens[i].score = 0.0;
	}
	*tokIdx = 0;
	if (inIdx < 0) return;
	for (size_t i=0; i+2<n; i++) {
		if (*tokIdx == TOKENS_COUNT) break;
		if (input[inIdx] == buffer[i] && input[inIdx+1] == buffer[i+1] && input[inIdx+2] == buffer[i+2]) {
			int tokenExists = 0;
			for (int j=0; j<TOKENS_COUNT; j++) {
				if (tokens[j].character == buffer[i+3]) {
					tokenExists = 1;
	
					int doesMatch = 1;
					int matchCount = 1;
					int checkIdx = inIdx;
					double thisScore = 1.0;
					while (doesMatch == 1) {
						checkIdx--;
						if (checkIdx < 0) break;
						if ((size_t)matchCount > i) break;
						if (input[checkIdx] == buffer[i-matchCount]) {
							matchCount++;
							thisScore *= PREV_CHAR_MATCH_GRANT_MUL;
						} else doesMatch = 0;
					}
					tokens[j].score += thisScore;
				}
			}
			if (tokenExists == 0) {
				int newIdx = *tokIdx;
				tokens[newIdx].character = buffer[i+3];
				tokens[newIdx].score++;
				(*tokIdx)++;

				int doesMatch = 1;
				int matchCount = 1;
				int checkIdx = inIdx;
				double thisScore = 1.0;
				while (doesMatch == 1) {
					checkIdx--;
					if (checkIdx < 0) break;
					if (input[checkIdx] == buffer[i-matchCount]) {
						doesMatch = 1;
						matchCount++;
						thisScore *= PREV_CHAR_MATCH_GRANT_MUL;
					} else doesMatch = 0;
				}
				tokens[newIdx].score += thisScore;
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
	double biggestScore = 0.0;
	double scoreSum = 0.0;
	for (int i=0; i<tokIdx; i++) {
		if (tokens[i].score > biggestScore) biggestScore = tokens[i].score;
		scoreSum += tokens[i].score;
		expos[i] = 0.0;
	}
	for (int i=0; i<tokIdx; i++) {
		expos[i] = exp((tokens[i].score-biggestScore+PRECISION_SHIFT)/(temp*scoreSum));
		expoSum += expos[i];
	}
	for (int i=0; i<tokIdx; i++) {
		outputs[i].chance = expos[i]/expoSum;
	}
}

void pickNextChar(char* input, int tokIdx, double chanceSum, int* inIdx, struct output outputs[], char *character) {
	if (tokIdx == 0) return;

	int nullIdx = *inIdx+3;

	double random = (double)rand()*chanceSum / (double)(RAND_MAX);
	double cumulative = 0.0;
	for (int i=0; i<tokIdx; i++) {
		cumulative += outputs[i].chance;
		if (random < cumulative) {
			*character = outputs[i].character;
			break;
		}
	}
	if (*character == '\0') *character = outputs[tokIdx-1].character;

	input[nullIdx] = *character;
	input[nullIdx+1] = '\0';
	(*inIdx)++;
}

int run3LM(size_t n, char buffer[]) {
	printf("You > ");

	int inIdx = -4;
	char input[INPUT_BUFFER_SIZE] = {0};
	if (fgets(input, sizeof(input), stdin) == NULL) return 1;
	for (int i=0; i<INPUT_BUFFER_SIZE; i++) {
		if (input[i] != '\0') inIdx++;
		else break;
	}

	printf("3LM > ");

	for (int i=0; i<INPUT_BUFFER_SIZE-1 && inIdx+4 < INPUT_BUFFER_SIZE; i++) {
		clock_t start = clock();
	
		struct token tokens[TOKENS_COUNT];
		struct output outputs[TOKENS_COUNT];
		int tokIdx = 0;
		nextTokens(n, input, inIdx, buffer, tokens, &tokIdx);
		if (tokIdx == 0) break;
		ReLU(tokens, tokIdx, TEMPERATURE, outputs);
	
		double totalChance = 0.0;
		for (int i=0; i<tokIdx; i++) {
			totalChance += outputs[i].chance;
		}

		char character = '\0';
		pickNextChar(input, tokIdx, totalChance, &inIdx, outputs, &character);
	
		clock_t afterCompute = clock();
		double msec = (afterCompute-start)*1000.0/CLOCKS_PER_SEC;
	
		//printf("GUESSES:\n");
		//for (int i=0; i<tokIdx; i++) {
		//	printf("CHAR: '%c' | SCORE: %d | CHANCE: %.10lf\n", tokens[i].character, tokens[i].score, outputs[i].chance);
		//}
		//printf("CHANCES SUM: %0.10lf (Off by %0.10lf)\n", totalChance, totalChance-1.0);
		//printf("COMPUTE TIME: %0.5lf ms\n", msec);
		//printf("\n\n\n\n TEXT: %s \n\n\n\n", input);
		printf("%c", character);
	}
	printf("\n");

	return 0;
}

int main(int argc, char *argv[]) {
	clock_t start = clock();
	srand((unsigned)time(NULL));

	FILE *f = fopen(argv[1], "r");
	char buffer[DATASET_BUFFER_SIZE];
	size_t n = 0;
	if (getText(buffer, f, &n) != 0) return 1;
	//printf(buffer);
	
	clock_t afterText = clock();
	double msec = (afterText-start)*1000.0/CLOCKS_PER_SEC;
	printf("Opened and read %s in %0.5f ms\n", argv[1], msec);
	
	while (1) {
		if (run3LM(n, buffer) != 0) return 1;
	}
}
