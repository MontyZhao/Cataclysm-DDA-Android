/*
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "../libintl.h"

#include <map>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32) || defined(WINCE)
typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif

#ifdef __ANDROID__
#include <iostream>
#endif

#include "MessageCatalog.hpp"
#include "Util.hpp"

using namespace std;

using namespace libintllite;
using namespace libintllite::internal;

static char* currentDefaultDomain = NULL;
static map<char*, MessageCatalog*> loadedMessageCatalogPtrsByDomain;

libintl_lite_bool_t loadMessageCatalog(const char* domain, const char* moFilePath)
{
	try
	{
		if (sizeof(uint32_t) != 4)
		{
			return LIBINTL_LITE_BOOL_FALSE;
		}

		if (!moFilePath || !domain)
		{
			return LIBINTL_LITE_BOOL_FALSE;
		}

		FILE* moFile = NULL;
		CloseFileHandleGuard closeFileHandleGuard(moFile);
		moFile = fopen(moFilePath, "rb");
		if (!moFile)
		{
			return LIBINTL_LITE_BOOL_FALSE;
		}

		uint32_t magicNumber;
		if (!readUIn32FromFile(moFile, false, magicNumber)) return LIBINTL_LITE_BOOL_FALSE;
		if ((magicNumber != 0x950412de) && (magicNumber != 0xde120495)) return LIBINTL_LITE_BOOL_FALSE;

		uint32_t fileFormatRevision;
		if (!readUIn32FromFile(moFile, false, fileFormatRevision)) return LIBINTL_LITE_BOOL_FALSE;
		if (fileFormatRevision != 0) return LIBINTL_LITE_BOOL_FALSE;

		bool needsBeToLeConversion = isBigEndian();

		uint32_t numberOfStrings;
		if (!readUIn32FromFile(moFile, needsBeToLeConversion, numberOfStrings)) return false;
		if (numberOfStrings == 0)
		{
			return LIBINTL_LITE_BOOL_TRUE;
		}

		uint32_t offsetOrigTable;
		if (!readUIn32FromFile(moFile, needsBeToLeConversion, offsetOrigTable)) return LIBINTL_LITE_BOOL_FALSE;

		uint32_t offsetTransTable;
		if (!readUIn32FromFile(moFile, needsBeToLeConversion, offsetTransTable)) return LIBINTL_LITE_BOOL_FALSE;

		string* sortedOrigStringsArray = NULL;
		ArrayGurard<string> sortedOrigStringsArrayGuard(sortedOrigStringsArray);
		sortedOrigStringsArray = new string[numberOfStrings];
		if (!sortedOrigStringsArray)
		{
			return LIBINTL_LITE_BOOL_FALSE;
		}

		if (!loadMoFileStringsToArray(moFile,
				numberOfStrings,
				offsetOrigTable,
				needsBeToLeConversion,
				sortedOrigStringsArray)) return LIBINTL_LITE_BOOL_FALSE;

		string* translatedStringsArray = NULL;
		ArrayGurard<string> translatedStringsArrayGuard(translatedStringsArray);
		translatedStringsArray = new string[numberOfStrings];
		if (!translatedStringsArray)
		{
			return LIBINTL_LITE_BOOL_FALSE;
		}

		if (!loadMoFileStringsToArray(moFile,
				numberOfStrings,
				offsetTransTable,
				needsBeToLeConversion,
				translatedStringsArray)) return LIBINTL_LITE_BOOL_FALSE;

		MessageCatalog* newMessageCatalogPtr = new MessageCatalog(numberOfStrings,
				sortedOrigStringsArray,
				translatedStringsArray);
		if (!newMessageCatalogPtr) return LIBINTL_LITE_BOOL_FALSE;
		sortedOrigStringsArrayGuard.release();
		translatedStringsArrayGuard.release();

		char* domainDup = strdup(domain);
		if (!domainDup) return LIBINTL_LITE_BOOL_FALSE;
		closeLoadedMessageCatalog(domain);
		loadedMessageCatalogPtrsByDomain[domainDup] = newMessageCatalogPtr;

		return LIBINTL_LITE_BOOL_TRUE;
	}
	catch (...)
	{
		return LIBINTL_LITE_BOOL_FALSE;
	}
}

void closeLoadedMessageCatalog(const char* domain)
{
	if (domain)
	{
		for (map<char*, MessageCatalog*>::iterator i = loadedMessageCatalogPtrsByDomain.begin();
				i != loadedMessageCatalogPtrsByDomain.end();
				i++)
		{
			if (strcmp(i->first, domain) == 0)
			{
				free(i->first);
				delete i->second;
				loadedMessageCatalogPtrsByDomain.erase(i);
				return;
			}
		}
	}
}

void closeAllLoadedMessageCatalogs()
{
	for (map<char*, MessageCatalog*>::iterator i = loadedMessageCatalogPtrsByDomain.begin();
			i != loadedMessageCatalogPtrsByDomain.end();
			i++)
	{
		free(i->first);
		delete i->second;
	}
	loadedMessageCatalogPtrsByDomain.clear();
	free(currentDefaultDomain);
	currentDefaultDomain = NULL;
}

const char* textdomain(const char* domain)
{
	if (domain)
	{
		char* newDefaultDomain = strdup(domain);
		if (!newDefaultDomain)
		{
			return NULL;
		}
		free(currentDefaultDomain);
		currentDefaultDomain = newDefaultDomain;
		return newDefaultDomain;
	}
	else
	{
		return NULL;
	}
}

const char* gettext(const char* origStr)
{
	return dgettext(NULL, origStr);
}

const char* dgettext(const char* domain, const char* origStr)
{
	if (!origStr)
	{
		return NULL;
	}

	if (!domain)
	{
		if (currentDefaultDomain)
		{
			domain = currentDefaultDomain;
		}
		else
		{
			return origStr;
		}
	}

	const MessageCatalog* msgCat = NULL;
	for (map<char*, MessageCatalog*>::iterator i = loadedMessageCatalogPtrsByDomain.begin();
			!msgCat && (i != loadedMessageCatalogPtrsByDomain.end());
			i++)
	{
		if (strcmp(i->first, domain) == 0)
		{
			msgCat = i->second;
		}
	}

	if (!msgCat)
	{
		return origStr;
	}

	const string* translatedStrPtr = msgCat->getTranslatedStrPtr(origStr);
	if (translatedStrPtr)
	{
		return translatedStrPtr->c_str();
	}
	else
	{
		return origStr;
	}
}

const char* ngettext(const char* origStr, const char* origStrPlural, unsigned long n)
{
#ifdef __ANDROID__
    try {
        // First let's get the translated megastring that contains all the plural translations separated by NUL chars
        const char* translatedStr = gettext(origStr);

        // Now determine correct plural form to use. This data is described in .PO files "Plural-Forms" metadata.
        const char* language = getenv("LANGUAGE");

        // Bail out if we don't have data, or the result of gettext is the exact same pointer to the source string (so we don't have a translated megastring to look at)
        if (!translatedStr || !language || translatedStr == origStr) {
            if (n == 1)
                return origStr;
            else
                return origStrPlural;
        }

        int nplurals=2;
        int plural=n!=1;
        if (strcmp(language, "en") == 0)            { nplurals=2; plural=(n != 1); }
        else if (strcmp(language, "fr") == 0)       { nplurals=2; plural=(n > 1); }
        else if (strcmp(language, "de") == 0)       { nplurals=2; plural=(n != 1); }
        else if (strcmp(language, "it_IT") == 0)    { nplurals=2; plural=(n != 1); }
        else if (strcmp(language, "es_AR") == 0)    { nplurals=2; plural=(n != 1); }
        else if (strcmp(language, "es_ES") == 0)    { nplurals=2; plural=(n != 1); }
        else if (strcmp(language, "ja") == 0)       { nplurals=1; plural=0; }
        else if (strcmp(language, "ko") == 0)       { nplurals=1; plural=0; }
        else if (strcmp(language, "pt_BR") == 0)    { nplurals=2; plural=(n > 1); }
        else if (strcmp(language, "pt_PT") == 0)    { nplurals=2; plural=n != 1; }
        else if (strcmp(language, "ru") == 0)       { nplurals=4; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<12 || n%100>14) ? 1 : n%10==0 || (n%10>=5 && n%10<=9) || (n%100>=11 && n%100<=14)? 2 : 3); }
        else if (strcmp(language, "zh_CN") == 0)    { nplurals=1; plural=0; }
        else if (strcmp(language, "zh_TW") == 0)    { nplurals=1; plural=0; }

        // Now we know which plural form to use, move the string pointer past the appropriate # of NUL characters.
        // This is a bit scary because it relies on the .MO file always storing the correct number of plurals,
        // otherwise we could overrun the string buffer. Seems to work fine though... (nervous laughter)
        for (int i = 0; i < plural; ++i)
            translatedStr += strlen(translatedStr) + 1; // advance to the start of the next string
        return translatedStr;
    } catch (const std::exception &err) {
        std::cerr << "Error in libintl-lite ngettext(): " << err.what() << std::endl;
        if (n == 1)
            return origStr;
        else
            return origStrPlural;
    }
#else
    if (n == 1)
    {
        return gettext(origStr);
    }
    else
    {
        return gettext(origStrPlural);
    }
#endif
}

const char* dngettext(const char* domain, const char* origStr, const char* origStrPlural, unsigned long n)
{
#ifdef __ANDROID__
    // not implemented yet
    return NULL;
#else
	if (n == 1)
	{
		return dgettext(domain, origStr);
	}
	else
	{
		return dgettext(domain, origStrPlural);
	}
#endif
}

#ifdef __ANDROID__
libintl_lite_bool_t bindtextdomain(const char* domain, const char* moFilePath)
{
	return loadMessageCatalog( domain, moFilePath );
}

libintl_lite_bool_t bind_textdomain_codeset(const char* domain, const char* oFilePath)
{
	// not implemented yet
}
#endif