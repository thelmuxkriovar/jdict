#include <iostream>
#include <form.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <locale>
#include <curl/curl.h>
#include "json.hpp"

using namespace std;
using json = nlohmann::basic_json<>;

string urlencode(const std::string& str) {
	/* urlencode by elnormous */
	static const char hexChars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	std::string result;

	for (auto i = str.begin(); i != str.end(); ++i) {
		uint8_t cp = *i & 0xFF;

		if ((cp >= 0x30 && cp <= 0x39) || // 0-9
			(cp >= 0x41 && cp <= 0x5A) || // A-Z
			(cp >= 0x61 && cp <= 0x7A) || // a-z
			cp == 0x2D || cp == 0x2E || cp == 0x5F) // - . _
			result += static_cast<char>(cp);
		else if (cp <= 0x7F) // length = 1
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
		else if ((cp >> 5) == 0x6) { // length = 2
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
			if (++i == str.end()) break;
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
		} else if ((cp >> 4) == 0xe) { // length = 3
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
			if (++i == str.end()) break;
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
			if (++i == str.end()) break;
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
		} else if ((cp >> 3) == 0x1e) { // length = 4
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
			if (++i == str.end()) break;
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
			if (++i == str.end()) break;
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
			if (++i == str.end()) break;
			result += std::string("%") + hexChars[(*i & 0xF0) >> 4] + hexChars[*i & 0x0F];
		}
	}

	return result;
}


size_t writeCurlOutToString(void *contents, size_t size, size_t nmemb, string *s) {
	size_t newLength = size*nmemb;
    try {
        s->append((char*)contents, newLength);
    } catch(std::bad_alloc &e) {
        return 0;
    }
    return newLength;
}

string doJishoQuery(string searchQuery) {
	CURL *curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();

	string out = "";
	if(curl) {
		string url = "https://jisho.org/api/v1/search/words?keyword=";
		url.append(urlencode(searchQuery));
    	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCurlOutToString);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_easy_cleanup(curl);
	}
	return out;
}

char* trim_whitespaces(char *str) {
	char *end;

	// trim leading space
	while(isspace(*str))
		str++;

	if(*str == 0) // all spaces?
		return str;

	// trim trailing space
	end = str + strnlen(str, 128) - 1;

	while(end > str && isspace(*end))
		end--;

	// write new null terminator
	*(end+1) = '\0';

	return str;
}


string getSearchQuery(FIELD *searchField) {
	string out(field_buffer(searchField, 0));

	out.erase(out.begin(), std::find_if(out.begin(), out.end(), [](int ch) { // ltrim
        return !std::isspace(ch);
    }));

	out.erase(std::find_if(out.rbegin(), out.rend(), [](int ch) { // rtrim
        return !std::isspace(ch);
    }).base(), out.end());

	return out;
}

template <class T> string implode(string glue, vector<T> vector) {
	string out;
	for(size_t i = 0; i < vector.size(); ++i) {
		if(i != 0)
			out.append(glue);
		out.append(vector[i]);
	}
	return out;
}

void mvprintwClean(int y, int x, const char* str) {
	move(y, x);
	clrtoeol();
	mvprintw(y, x, str);
	refresh();
}

void handleResults(string searchQuery, json searchResultParsed) {
	int y = 1;
	string searchResultTitle = "Search Results for: \"";
	searchResultTitle.append(searchQuery).append("\"");

	mvprintwClean(y++, 0, searchResultTitle.c_str());

	if(searchResultParsed["data"].size() < 1) {
		mvprintwClean(y++, 0, "No result found");
		return;
	}

	for(int i = 0; i < searchResultParsed["data"].size(); i++) {
		auto result = searchResultParsed["data"][i];

		string wordDescription = "";

		try { // "word" exists
			wordDescription
				.append(result["japanese"][0].at("word"))
				.append("【")
				.append(result["japanese"][0].at("reading"))
				.append("】");
		} catch(json::out_of_range& e) { // "word" or "reading" doesn't exist
			try {
				wordDescription = result["japanese"][0].at("reading");
			} catch(json::out_of_range& e2) { // word has no "reading" & no "word" ? ...
				wordDescription = result["japanese"][0].at("word");
			} catch(json::type_error &e2) { // this shouldn't happen -- nothing exists, just ignore it
				continue;
			}
		} catch(json::type_error &e) { // generic error I guess, just ignore the word
			continue;
		}

		try {
			if(result["is_common"])
				wordDescription.append(" (common word) ");
		} catch(json::type_error &e) { }

		if(result["senses"][0]["parts_of_speech"].size() > 0) {
			vector<string> partsOfSpeech = result["senses"][0]["parts_of_speech"];
			wordDescription.append(" *").append(implode<string>(", ", partsOfSpeech)).append("*");
		}

		mvprintwClean(y++, 0, wordDescription.c_str());

		vector<json> senses = result["senses"];

		if (senses.size() > 5) {
			senses.resize(5);

			mvprintwClean(y++, 0, "*This word has too many meanings*");
			mvprintwClean(y++, 0, "*Please visit jisho for more information*");
		}

		for(int j = 0; j < senses.size(); j++) {
			string out = "";
			auto sense = senses[j];
			auto englishDefintions = sense["english_definitions"];

			string tags = "";
			try {
				if(sense.at("tags").size() > 0) {
					tags
						.append(" (")
						.append(implode<string>(", ", sense.at("tags")))
						.append(")");
				}
			} catch(json::out_of_range& e) {}

			out
				.append(to_string(j + 1))
				.append(". \u200b")
				.append(implode<string>("; ", englishDefintions))
				.append(tags);
			mvprintwClean(y++, 0, out.c_str());
		}

		mvprintwClean(y++, 0, "________________");
	}


	//mvprintwClean(y++, 0, result.dump(4).c_str());
}

int main() {
	FIELD *searchFields[2];
	FORM  *searchForm;

	int ch;

	setlocale(LC_ALL, "");

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	searchFields[0] = new_field(1, max(COLS - 10, 25), 0, 8, 0, 0);
	searchFields[1] = NULL;

	set_field_back(searchFields[0], A_UNDERLINE | A_UNDERLINE);
	set_field_opts(searchFields[0], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);

	searchForm = new_form(searchFields);
	post_form(searchForm);
	set_current_field(searchForm, searchFields[0]);

	mvprintw(0, 0, "Search: ");
	refresh();

	FILE *f;
	string searchQuery, searchResult;
	json searchResultParsed;

	int cursorPositionInForm = 0;
	int maxY = LINES; // initialized from initscr()

	while((ch = getch()) != KEY_F(1)) {
		switch(ch) {
			case KEY_LEFT:
				form_driver(searchForm, REQ_PREV_CHAR);
			break;

			case KEY_ENTER: case 10:
				cursorPositionInForm = searchForm->curcol;

				// sometimes the buffer isn't set unless the field changes, so I force this change with a useless jump
				form_driver(searchForm, REQ_NEXT_FIELD);
				form_driver(searchForm, REQ_PREV_FIELD);

				// reset cursor to previous position after the jump
				searchForm->curcol = cursorPositionInForm;
				pos_form_cursor(searchForm);

				for(int i = 1; i < maxY; i++) {
					move(i, 0);
					clrtoeol();
				}
				mvprintw(1, 0, "Searching... Please wait");
				refresh();

				searchQuery = getSearchQuery(searchFields[0]);
				searchResult = doJishoQuery(searchQuery);
				searchResultParsed = json::parse(searchResult);

				handleResults(searchQuery, searchResultParsed);
			break;

			case KEY_RIGHT:
				form_driver(searchForm, REQ_NEXT_CHAR);
			break;

			case KEY_BACKSPACE: case 127:
				form_driver(searchForm, REQ_DEL_PREV);
			break;

			case KEY_DC: // DC is del (back delete)
				form_driver(searchForm, REQ_DEL_CHAR);
			break;

			default:
				form_driver(searchForm, ch);
			break;
		}

		refresh();
	}

	unpost_form(searchForm);
	free_form(searchForm);
	free_field(searchFields[0]);

	endwin();
	return 0;
}
