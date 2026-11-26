/* idioma.h — el idioma de la interfaz (v1.07). Los textos se escriben en español en el codigo y
   `tr()` los cambia por los del idioma elegido (`idioma = en|es|fr|de|zh|ja|ru` en serve.conf; ingles
   si no se dice nada). La tabla sale de tools/traducciones.py. */
#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace idioma {

enum Lengua { EN = 0, ES, FR, DE, ZH, JA, RU, kLenguas };

/* "en", "es", ... y el nombre de cada idioma en su propio idioma (English, Español, 中文...) */
const char *codigo(int l);
const char *nombre(int l);
/* -1 si el codigo no es de ningun idioma conocido */
int deCodigo(const std::string &c);

int actual();
void poner(int l);
/* lee ODYSSEYVK_IDIOMA o, si no esta, `idioma =` de ~/.config/odysseyvk/serve.conf, y lo pone */
int cargar();
/* lo que dice serve.conf ahora mismo (para notar un cambio hecho desde el panel) */
int leerDeConfig();

/* el texto en el idioma puesto; si la tabla no lo trae, sale tal cual (en español) */
const char *tr(const char *es);
/* igual, cambiando cada `{}` por el siguiente dato */
std::string trf(const char *es, std::initializer_list<std::string> datos);

/* los signos fuera del latino-1 que usa el idioma `l` (y los nombres de los siete idiomas, que el
   selector pinta siempre): el atlas de letras los carga aparte */
std::vector<unsigned> signosDe(int l);
/* la coma decimal del idioma: «1,5» en español, francés, alemán y ruso; «1.5» en inglés, chino y japonés */
char decimal();
/* chino o japones: hace falta el tipo CJK */
bool esCjk(int l);

}  // namespace idioma
