CC_FLAGS := -Wall -std=c99

sense_lps331ap:

%: %.c
	gcc $(CC_FLAGS) -o $@ $< 
