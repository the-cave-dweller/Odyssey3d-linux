/* ajuste.h — el canal de ajuste vivo entre la ventana de desarrollo y el visor que esta tejiendo.

   El visor se queda el teclado de la pantalla del panel (por eso la trama tapa el navegador), asi
   que los numeros solo se podian mover con las teclas dentro de esa ventana. Este canal rompe eso
   sin tocar el compositor: tres ficheros en ~/.local/state/odyssey3d.

     ajuste          lo escribe de fuera (la ventana Qt o `odyssey3d ctl ajuste`) y lo lee el visor
                     cada decima de segundo; `clave=valor`, una por linea, `#` comentarios.
     estado          lo escribe el visor dos veces por segundo: su valor REAL de cada numero, la red
                     que tiene abierta y cuanto tarda. La ventana de desarrollo lee de ahi.
     preferencias.log lo escribe el visor cada vez que un numero cambia, venga de donde venga
                     (tecla, pastilla,ctl o ventana Qt): hora, clave, valor anterior, valor nuevo y
                     la red puesta. Es la "ultima preferencia" de la que se puede hablar despues.

   Las claves son las mismas de serve.conf (fase, paso, filtro, crosstalk, conv, rampa_x, rampa_y,
   fondo, frente, empuje_fondo, empuje_frente, planos, muestras, temblor, canto, fase_temblor,
   relieve, apoyo) mas `modelo`, que es una ruta .onnx y pide cambiar de red en marcha. */
#ifndef ODYSSEY_AJUSTE_H
#define ODYSSEY_AJUSTE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ajuste {

/* Las claves que aceptan los numeros, en el orden en que las quiere ver una interfaz.
   `min`/`max` son topes sanos: fuera de ahi el sombreador o el presupuesto de la lente se rompen. */
struct Clave {
  const char *nombre;
  const char *etiqueta;   /* como se llama en la pastilla y en el README */
  double minimo;
  double maximo;
  double paso;            /* de cuanto en cuanto se mueve el deslizador */
  bool entero;            /* las muestras son 0..3, no 1,7 */
};
const std::vector<Clave> &claves();
const Clave *buscarClave(const std::string &nombre);

/* Lee el fichero de ajuste. Devuelve los numeros que aparecen (con la clave tal cual esta escrita)
   y en `modelo` la ruta .onnx pedida, si la hay. En `texto` (opcional) quedan las ORDENES que llevan
   palabra o ruta en vez de numero: `camara = /dev/video0`, `mono = auto`, `capa = ocultar` (esta la
   atiende el visor, no el vigilante). Falso si el fichero no existe (no es un error: el canal es
   opcional). */
bool leer(std::map<std::string, double> &numeros, std::string &modelo,
          std::map<std::string, std::string> *texto = nullptr);

/* El visor solo aplica el fichero cuando CAMBIO de verdad (hora de modificacion y tamano): si lo
   aplicara siempre, una tecla pulsada despues de escribir un numero se volveria a poner sola al
   valor que esta escrito. Los numeros del fichero son una orden, no la verdad. */
class Lector {
 public:
  bool cambio(std::map<std::string, double> &numeros, std::string &modelo,
              std::map<std::string, std::string> *texto = nullptr);

 private:
  long long marca_ = -1;
};

/* Hay lineas del fichero que no son un numero sino una ORDEN, y las atiende el vigilante (el hub),
   no el visor: `overlay = 1` pide el overlay montado EN EL ESCRITORIO del monitor, sin esperar a que
   nadie se ponga a pantalla completa, y `overlay = 0` lo quita; `camara = /dev/videoN` y
   `mono = auto|si|no` eligen la camara del Facetracking para la proxima sesion. El visor pasa de esas
   lineas en silencio (si no, las reportaria como un numero que no conoce). */
bool esOrden(const std::string &nombre);

/* El vigilante cuenta que sesion tiene abierta (ninguna / video / escritorio), en que pantalla y con
   que modo, en su propio fichero. La ventana de ajustes lo lee para saber si su boton esta puesto. */
void escribirEstadoHub(const std::string &texto);
std::string rutaHub();
std::map<std::string, std::string> leerEstadoHub();

/* Escribe el fichero de ajuste ENTERO con estos valores: lo que no se manda esta vez, se va. Es una
   ORDEN para el visor, no la memoria de lo mandado (si se arrastraran los mandatos anteriores, un numero
   que alguien movio despues con las teclas volveria atras solo). Se deja atomicamente en su sitio. Lo
   usan `ctl ajuste`, `ctl escritorio` y la ventana de ajustes. */
bool escribir(const std::map<std::string, double> &numeros, const std::string &modelo,
              std::string *err);
/* Igual, pero pudiendo mandar tambien las ORDENES de palabra (`camara`, `mono`). La ventana de
   ajustes la usa para decir con que camara quiere el Facetracking de la proxima sesion. */
bool escribir(const std::map<std::string, double> &numeros, const std::string &modelo,
              const std::map<std::string, std::string> &ordenes, std::string *err);

/* El visor cuenta lo que esta usando ahora mismo (una linea por numero, mas la red y el reloj). */
void escribirEstado(const std::string &texto);

/* Un cambio registrado: hora local, clave, antes -> ahora, y la red que estaba puesta. */
void registrar(const std::string &clave, const std::string &antes, const std::string &ahora,
               const std::string &modelo, const char *fuente);

/* Las rutas de los tres ficheros, por si alguien las necesita fuera (la ventana de ajustes). */
std::string rutaAjuste();
std::string rutaEstado();
std::string rutaHistorial();

/* Lo que el visor escribio en `estado`: pares `clave = valor` en texto crudo (la ventana los lee
   para pintar los numeros que el visor esta usando de verdad, no los que cree tener). */
std::map<std::string, std::string> leerEstado();

/* El texto del fichero de ajuste, para mostrarlo tal cual (ctl). */
std::string textoAjuste();
/* Las ultimas `n` lineas del historial, de la mas vieja a la mas nueva. */
std::vector<std::string> historial(int n);

/* ---------------- lo de fabrica y los guardados por red (desde v0.52) ----------------

   `valoresFabrica` es el punto de retorno del boton de revertir: los numeros con los que arranca el
   visor (los que trae el programa, corregidos con lo que dice serve.conf). No es una copia de lo que
   hay en el fichero de ajuste: ese es una orden de una vez y se vacia. */
std::map<std::string, double> valoresFabrica();
std::string rutaServeConf();

/* La vista de la camara que esta usando el visor: el visor vuelca ahi el ultimo cuadro crudo
   (JPEG tal cual lo solto la camara) una vez cada dos segundos, y la ventana de ajustes lo ensena.
   Se hace asi porque la camara del monitor la tiene abierta el visor: si la abriera tambien la
   ventana, la 04e8:20d7 dejarfa de responder. */
std::string rutaCamaraVista();
bool escribirCamaraVista(const std::vector<uint8_t> &jpeg, std::string *err = nullptr);

/* serve.conf en crudo (para ensenar que camara esta puesta, que `rastreo` esta a `si`...).
   `fijarEnServeConf` cambia UNA linea de ese fichero sin tocar el resto, y deja la anterior en
   `serve.conf.auterior`: es lo que hace la ventana cuando se elige otra camara. */
std::map<std::string, std::string> leerServeConf();
bool fijarEnServeConf(const std::string &clave, const std::string &valor, std::string *err);

/* Cuantas claves de `numeros` se salen de `base`, y cuales son (para decirlo en la ventana). */
int diferencias(const std::map<std::string, double> &numeros, const std::map<std::string, double> &base,
                std::vector<std::string> *nombres = nullptr);

/* Los numeros que se guardaron PARA CADA RED (.onnx), en `perfiles`. Solo interesa guardar los de una
   red que se modifico: revertir esa red es quitar su perfil y mandar los valores de fabrica. Fichero
   de texto, una seccion `[ruta=/donde/mde_large.onnx]` por red y una linea `clave = valor` dentro. */
std::string rutaPerfiles();
std::map<std::string, std::map<std::string, double>> leerPerfiles();
bool guardarPerfil(const std::string &modelo, const std::map<std::string, double> &numeros,
                   std::string *err);
bool borrarPerfil(const std::string &modelo, std::string *err);

/* Las redes que la ventana tiene a mano (la lista de la derecha). Se quedan entre arranques, y no
   tocan nada del visor: son solo una lista de rutas, una por linea. */
std::string rutaSeleccion();
std::vector<std::string> leerSeleccion();
bool guardarSeleccion(const std::vector<std::string> &rutas, std::string *err);

}  /* namespace ajuste */

#endif
