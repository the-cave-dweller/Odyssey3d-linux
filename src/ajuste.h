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
   y en `modelo` la ruta .onnx pedida, si la hay. Falso si el fichero no existe (no es un error: el
   canal es opcional). */
bool leer(std::map<std::string, double> &numeros, std::string &modelo);

/* El visor solo aplica el fichero cuando CAMBIO de verdad (hora de modificacion y tamano): si lo
   aplicara siempre, una tecla pulsada despues de escribir un numero se volveria a poner sola al
   valor que esta escrito. Los numeros del fichero son una orden, no la verdad. */
class Lector {
 public:
  bool cambio(std::map<std::string, double> &numeros, std::string &modelo);

 private:
  long long marca_ = -1;
};

/* Hay lineas del fichero que no son un numero sino una ORDEN, y las atiende el vigilante (el hub),
   no el visor: `overlay = 1` pide el overlay montado EN EL ESCRITORIO del monitor, sin esperar a que
   nadie se ponga a pantalla completa, y `overlay = 0` lo quita. El visor pasa de esas lineas en
   silencio (si no, las reportaria como un numero que no conoce). */
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

}  /* namespace ajuste */

#endif
