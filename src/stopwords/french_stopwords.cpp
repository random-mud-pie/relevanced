#include <string>
#include <unordered_set>
#include "stopwords/french_stopwords.h"

using namespace std;

namespace relevanced {
namespace stopwords {

const unordered_set<string> stopwordSet {
  "au",
  "aux",
  "avec",
  "ce",
  "ces",
  "dans",
  "de",
  "des",
  "du",
  "elle",
  "en",
  "et",
  "eux",
  "il",
  "je",
  "la",
  "le",
  "leur",
  "lui",
  "ma",
  "mais",
  "me",
  "m\u00eame",
  "mes",
  "moi",
  "mon",
  "ne",
  "nos",
  "notre",
  "nous",
  "on",
  "ou",
  "par",
  "pas",
  "pour",
  "qu",
  "que",
  "qui",
  "sa",
  "se",
  "ses",
  "son",
  "sur",
  "ta",
  "te",
  "tes",
  "toi",
  "ton",
  "tu",
  "un",
  "une",
  "vos",
  "votre",
  "vous",
  "c",
  "d",
  "j",
  "l",
  "\u00e0",
  "m",
  "n",
  "s",
  "t",
  "y",
  "\u00e9t\u00e9",
  "\u00e9t\u00e9e",
  "\u00e9t\u00e9es",
  "\u00e9t\u00e9s",
  "\u00e9tant",
  "\u00e9tante",
  "\u00e9tants",
  "\u00e9tantes",
  "suis",
  "es",
  "est",
  "sommes",
  "\u00eates",
  "sont",
  "serai",
  "seras",
  "sera",
  "serons",
  "serez",
  "seront",
  "serais",
  "serait",
  "serions",
  "seriez",
  "seraient",
  "\u00e9tais",
  "\u00e9tait",
  "\u00e9tions",
  "\u00e9tiez",
  "\u00e9taient",
  "fus",
  "fut",
  "f\u00fbmes",
  "f\u00fbtes",
  "furent",
  "sois",
  "soit",
  "soyons",
  "soyez",
  "soient",
  "fusse",
  "fusses",
  "f\u00fbt",
  "fussions",
  "fussiez",
  "fussent",
  "ayant",
  "ayante",
  "ayantes",
  "ayants",
  "eu",
  "eue",
  "eues",
  "eus",
  "ai",
  "as",
  "avons",
  "avez",
  "ont",
  "aurai",
  "auras",
  "aura",
  "aurons",
  "aurez",
  "auront",
  "aurais",
  "aurait",
  "aurions",
  "auriez",
  "auraient",
  "avais",
  "avait",
  "avions",
  "aviez",
  "avaient",
  "eut",
  "e\u00fbmes",
  "e\u00fbtes",
  "eurent",
  "aie",
  "aies",
  "ait",
  "ayons",
  "ayez",
  "aient",
  "eusse",
  "eusses",
  "e\u00fbt",
  "eussions",
  "eussiez",
  "eussent"
};

bool isFrenchStopword(const string &word) {
  return stopwordSet.count(word) > 0;
}

} // stopwords
} // relevanced

