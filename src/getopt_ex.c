#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include "getopt_ex.h"
#include "utils.h"
#include "ansi.h"

static int processing = 0;

/**
 * @brief Frees memory automatically when used with __attribute__((cleanup))
 * @param ptr Pointer to the memory to be freed
 * @note This function is intended to be used with the 'autoptr' macro defined in utils.h
 */
static void free_(void* ptr) {
	if( ptr ) free(*(void**)ptr);
}

/**
 * @brief Validates the given array of getopt_ex_option_t structures.
 * @param options Pointer to the array of options to validate.
 * @note This function checks for invalid characters, reserved characters, and duplicate option characters.
 */
static void validate_options(const getopt_ex_option_t* options) {
	uint8_t charset[256] = {0};

	if( !options ) {
		a_error("Error: options array is null\n");
		exit(1);
	}

	for( const getopt_ex_option_t* opt = options; opt->opt.name; opt++ ) {
		// Check that option characters are valid (letters or 0 for no short option)
		if( ! isalpha(opt->opt.val) ) {
			a_error("Error: Invalid option character '%c'\n", opt->opt.val);
			exit(1);
		}

		// make sure options don't use reserved characters and that there are no duplicates
		if( opt->opt.val == 'h' ) {
			a_error("Error: Option character 'h' is reserved for help\n");
			exit(1);
		}

		// check for duplicates
		if( charset[opt->opt.val] ) {
			a_error("Error: Duplicate option character '%c'\n", opt->opt.val);
			exit(1);
		}
		charset[opt->opt.val] = 1;
	} // end for
}

/**
 * @brief Prints the help information for the command-line options.
 * @param appname The name of the application.
 * @param options Pointer to the array of getopt_ex_option_t structures.
 * @param description The description of the application.
 */
static void print_help(const char* appname, const getopt_ex_option_t* options, const char* description) {
	static const char* indent = "  ";

	// description
	if( description ) {
		for( const char* p = description; *p; p++ ) {
			if( *p == '\n' ) {
				printf("\n%s", indent);
			} else if( *p == '\r' ) {
				// ignore
			} else {
				fputc(*p, stdout);
			}
		}
		fputs("\n\n", stdout);
	}

	// usage
	char usage[128] = {0};
	printf("Usage:\n");
	snprintf(usage, sizeof(usage), "%s%s [options]", indent, appname);
	printf("%s\n\n", usage);

	// options
	printf("Options:\n");
	for( const getopt_ex_option_t* opt = options; opt->opt.name; opt++ ) {
		printf("%s-%c|--%s", indent, opt->opt.val, opt->opt.name);
		if( opt->opt.has_arg == required_argument ) {
			printf(" <%s>", opt->argname ? opt->argname : "arg");
		}
		printf("\n%s%s%s\n\n", indent, indent, opt->description ? opt->description : "");
	}
}

/**
 * @brief qsort callback to sort options by their option character (case-insensitive)
 * @param a Pointer to the first option
 * @param b Pointer to the second option
 * @return int Negative if a < b, zero if a == b, positive if a > b
 */
// qsort callback to sort options by their option character (case-insensitive)
static int qsortCallback(const void* a, const void* b) {
	const getopt_ex_option_t* optA = (const getopt_ex_option_t*)a;
	const getopt_ex_option_t* optB = (const getopt_ex_option_t*)b;
	return tolower(optA->opt.val) - tolower(optB->opt.val);
}

/**
 * @brief Get the opt ex object
 * 
 * @param argc The number of command-line arguments
 * @param argv The array of command-line arguments
 * @param long_options Pointer to the array of getopt_ex_option_t structures
 * @param description The description of the application
 * @param callback The callback function to handle each option and positional argument
 */
void getopt_ex(
	int argc,
	char* const *argv,
	const getopt_ex_option_t* long_options,
	const char* description,
	void(*callback)(int pos, int opt, const char* optarg)
) {
	if( processing ) {
		a_error("Error: getopt_ex is not reentrant and cannot be called recursively\n");
		exit(1);
	}
	processing++; // set flag to prevent reentrancy

	static const getopt_ex_option_t help_option = {{"help", no_argument, 0, 'h'}, "Show this help information", 0};
	static const getopt_ex_option_t termination_option = {{0, 0, 0, 0}, 0, 0};
	char optstr[256] = {0};
	size_t num_options = 0;
	autoptr(free_) getopt_ex_option_t* alloptions = 0;
	autoptr(free_) struct option* getopt_options = 0;

	// reset state of getopt in case it was called before,
	// to allow multiple calls to getopt_ex in the same program
	optind = 0;
#ifdef __FreeBSD__
	optreset = 1; // Required for BSD/macOS systems
#endif

#ifndef _NDEBUG
	if( ! callback ) {
		a_error("Error: callback function is null\n");
		exit(1);
	}
	validate_options(long_options);
#endif

	// count the number of options in the given long_options array
	for(const getopt_ex_option_t* opt = long_options; opt->opt.name; opt++ ) {
		num_options++;
	}
	num_options++; // add the help option to the count

	// allocate memory for all options (including the help option)
	alloptions = (getopt_ex_option_t*)calloc(num_options + 1, sizeof(getopt_ex_option_t));
	getopt_options = (struct option*)calloc(num_options + 1, sizeof(struct option));

	// populate both arrays with the options from long_options and the help option
	for( const getopt_ex_option_t* opt = long_options; opt->opt.name; opt++ ) {
		int i = opt - long_options; // calculate the index based on the pointer arithmetic
		alloptions[i] = *opt;
		getopt_options[i] = opt->opt;
	}
	// add help option at the end
	alloptions[num_options - 1] = help_option;
	getopt_options[num_options - 1] = help_option.opt;
	// terminate both arrays
	alloptions[num_options] = termination_option;
	getopt_options[num_options] = termination_option.opt;

	// sort the alloptions array by the option character to ensure consistent
	// ordering in the help output and to make it easier to find options when parsing
	qsort(alloptions, num_options, sizeof(getopt_ex_option_t), qsortCallback);

	// build the options string for getopt_long
	strcat(optstr, ":"); // start with ':' to enable silent error reporting
	for( const getopt_ex_option_t* opt = alloptions; opt->opt.name; opt++ ) {
		strncat(optstr, (const char*)&opt->opt.val, 1);
		if( opt->opt.has_arg == required_argument ) {
			strcat(optstr, ":");
		}
	}

	// disable getopt's own error messages and let us handle them with our own formatting
	extern int opterr;
	opterr = 0;

	// Parse the command-line arguments using getopt_long
	int opt;
	while( (opt = getopt_long(argc, argv, optstr, getopt_options, 0)) != -1 ) {
		switch( opt ) {
			case 0: {
				// This means a long option was found, but getopt_long doesn't return the option
				// character for long options without a short equivalent. We need to find the
				// corresponding option in our alloptions array
				const char* long_opt_name = argv[optind - 1] + 2; // skip '--'
				const getopt_ex_option_t* found_opt = NULL;
				for( size_t i = 0; i < num_options; i++ ) {
					if( strcmp(alloptions[i].opt.name, long_opt_name) == 0 ) {
						found_opt = &alloptions[i];
						break;
					}
				}
				if( found_opt ) {
					callback(-1, found_opt->opt.val, optarg);
				} else {
					a_error("Unknown option: --%s\n", long_opt_name);
					exit(1);
				}
				break;
			}
			case ':': {
				a_error("Option -%c requires an argument\n", optopt);
				exit(1);
			}
			case '?': {
				a_error("Unknown option: -%c\n", optopt);
				exit(1);
			}
			case 'h': {
				print_help(basename(argv[0]), alloptions, description);
				exit(0);
			}
			default: {
				callback(-1, opt, optarg);
				break;
			}
		} // end switch
	} // end while

	// Handle positional arguments
	int positional_index = 0;
	while( optind < argc ) {
		callback(positional_index++, 0, argv[optind++]);
	}

	processing--; // reset flag to allow future calls
}
