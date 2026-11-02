# Guion de narración — Master of Darkness

Todas las pantallas de texto del juego, en el orden en que salen, para generar las voces con una IA de voz.
Cada página tiene un **código** (N01, N02…): es el nombre que tendrá su archivo de voz en el pack de narración
(`N01.ogg`, o `N01 - lo que quieras.ogg`, igual que el pack de música).

- **Inglés:** texto original del juego (Sega, 1992), pasado a frases normales.
- **Español:** nuestra traducción, la que lleva la app (`tools/traduccion/es.txt`), con puntuación normal para
  que la voz lo lea bien. Es texto propio: no hace falta pedir permiso a nadie para grabarla.
- El texto entre corchetes es una indicación para la voz, no se lee.
- La columna "ROM" es la dirección de la página en la ROM: la app la usará para reconocer qué página está en
  pantalla.

Sugerencia de voces: un **narrador** para casi todo y otra voz, maliciosa, para **el conde Massen**
(N14–N16: es él quien habla, aunque no se descubre hasta N21).

---

## 1. Introducción (antes del título, con la música 87)

| Código | ROM | Inglés | Español |
|:-:|:-:|---|---|
| N01 | DD1C | ...And the legend goes... | ...Y cuenta la leyenda... |
| N02 | DD48 | ...that the full moon... | ...Que la luna llena... |
| N03 | DD73 | ...brings with it... | ...Trae consigo... |
| N04 | DD9A | ...horrible events... | ...Sucesos atroces... |
| N05 | E299 | Have you not heard the legend of he who rises with the full moon... the legend of the vampire? | ¿No has oído la leyenda del que se alza con la luna llena... la leyenda del vampiro? |
| N06 | E317 | Well, the story actually happened in England... 100 years ago. | Pues bien, esta historia ocurrió de verdad en Inglaterra... hace 100 años. |
| N07 | E3BC | There was an investigator of the paranormal, a young psychologist named Ferdinand Social. | Había un investigador de lo paranormal, un joven psicólogo llamado Ferdinand Social. |
| N08 | E434 | ...On the night of the full moon, he received a mysterious message while using his Ouija board. | ...Una noche de luna llena, recibió un mensaje misterioso mientras usaba su tablero ouija. |

Después la tabla de ouija deletrea letra a letra sus palabras. Son palabras sueltas y van con efecto de sonido;
si quieres voz también, serían estas (CUIDADO sale dos veces seguidas: con una voz basta):

| Código | Inglés | Español (lo que deletrea el tablero) |
|:-:|---|---|
| N09a | Killer | Crimen |
| N09b | Vampire | Vampiro |
| N09c | Go to Thames | Al Támesis |
| N09d | Caution | Cuidado |
| N09e | In the wake of | Tras el rastro |
| N09f | Dracula | De Drácula |

## 2. Al empezar la partida (antes de la ronda 1)

| Código | ROM | Inglés | Español |
|:-:|:-:|---|---|
| N10 | D081 | At that time in London, the rising of the full moon meant the coming of yet another gruesome murder. | En aquella época, en Londres, cada luna llena anunciaba un nuevo y espantoso asesinato. |
| N11 | D100 | The following morning would always bring the discovery of yet another body... completely drained of blood. | A la mañana siguiente siempre aparecía otro cuerpo sin una sola gota de sangre. |
| N12 | D1B5 | ...Dr. Social knew that there was something supernatural at the heart of the mystery, and began investigating. | ...El Dr. Social sabía que en el fondo del misterio había algo sobrenatural, y se puso a investigar. |
| N13 | D25B | Calling on the powers of the Ouija board, Social learned that at the Thames River, horrible events beyond human imagination were taking place. | Invocando los poderes del tablero ouija, Social supo que en el río Támesis ocurrían horrores que iban más allá de la imaginación humana. |

## 3. Tras la ronda 1 (habla el conde Massen)

| Código | ROM | Inglés | Español |
|:-:|:-:|---|---|
| N14 | D329 | [risa] Heh heh heh... So you managed to defeat my Jack the Ripper. That's right... all that has happened here has been under my control. | [risa] Je, je, je... Así que has vencido a mi Jack el Destripador. Pues sí... todo lo que ha ocurrido aquí ha estado bajo mi control. |
| N15 | D3BB | And the things you've seen so far are just the start of the horrors...! [risa] Heh heh heh... Go to the House of Wax, if you dare. | ¡Y lo que has visto hasta ahora solo es el comienzo del horror...! [risa] Je, je, je... Ve al museo de cera, si te atreves. |
| N16 | D46C | My waxwork figures will be waiting for you! Perhaps you will be my next wax masterpiece. I need a replacement for Jack the Ripper, after all. | ¡Mis figuras de cera te estarán esperando! Quizá tú seas mi próxima obra maestra de cera. Al fin y al cabo, necesito un sustituto para Jack el Destripador. |

## 4. Tras la ronda 2

| Código | ROM | Inglés | Español |
|:-:|:-:|---|---|
| N17 | D535 | [susurrando] "...The cemetery, east of town..." [narrador] After defeating the evil spirits, the psychic girl managed to gasp a warning before falling into unconsciousness. | [susurrando] «...El cementerio, al este...» [narrador] Tras vencer a los espíritus malignos, la joven médium logró susurrar un aviso antes de desmayarse. |
| N18 | D5DA | Jack the Ripper was collecting bodies and fresh blood, to be spilt as sacrificial offerings to dark forces. | Jack el Destripador reunía cuerpos y sangre fresca para derramarla como ofrenda a las fuerzas oscuras. |
| N19 | D68B | [susurrando] "...The cemetery, east of town..." [narrador] There was no way to know what the message was warning him about... | [susurrando] «...El cementerio, al este...» [narrador] No había forma de saber qué anunciaba aquel aviso... |
| N20 | D719 | But Dr. Social had to act quickly, so he rushed there as fast as he could. | Pero el Dr. Social debía actuar rápido, así que corrió hacia allí sin demora. |


## 5. Tras la ronda 3

| Código | ROM | Inglés | Español |
|:-:|:-:|---|---|
| N21 | D7AC | The strangely familiar figure who was floating in mid-air was Count Massen! | La figura que flotaba en el aire le era familiar... ¡Era el conde Massen! |
| N22 | D80C | There had been rumors about the Count, who lived in an old castle, far from civilization. It was said that he was studying the black arts. | Se rumoreaba que el conde vivía en un viejo castillo, lejos de toda civilización, y que estudiaba las artes oscuras. |
| N23 | D8A9 | Yes, it seemed that the Count was indeed involved in strange doings. | Sí, parecía que el conde andaba metido en asuntos muy extraños. |
| N24 | D946 | [pensativo] "...Black magic... full moon... killer...?" [narrador] Finally understanding the evil power behind the events, Dr. Social hurried to the castle. | [pensativo] «...Magia negra... luna llena... ¿crimen?» [narrador] Al fin comprendió el poder maligno que había detrás, y el Dr. Social corrió al castillo. |

## 6. Tras la ronda 4

| Código | ROM | Inglés | Español |
|:-:|:-:|---|---|
| N25 | D9EB | A strange white light came from Count Massen's fingertip, and the next thing Dr. Social knew was that he was imprisoned inside a dark cage. | Una extraña luz blanca brotó del dedo del conde Massen. Lo siguiente que supo el Dr. Social es que estaba encerrado en una jaula oscura. |
| N26 | DA87 | Dr. Social threw himself against the door in desperation. Again and again he tried, until the door gave way. | Desesperado, el Dr. Social se lanzó contra la puerta. Lo intentó una y otra vez, hasta que la puerta cedió. |
| N27 | DB37 | But now he found himself in the middle of a labyrinth of incredible complexity. | Pero ahora se encontraba en medio de un laberinto increíblemente complejo. |

## 7. Final (texto que sube por la pantalla, tras vencer a Drácula)

Es un solo texto largo que se desplaza; está partido en párrafos para que cada voz no sea demasiado larga.
La app los irá encadenando.

| Código | Inglés | Español |
|:-:|---|---|
| N30 | With the rising sun, the darkness began to change to light. Social reached the top of a small ridge and stopped. He looked at the outside of Castle Dracula at last. | Con el sol naciente, la oscuridad se tornó en luz. Social llegó a lo alto de una loma y se detuvo. Por fin veía el castillo de Drácula desde fuera. |
| N31 | This was Transylvania, just as Count Massen had described it. The scenery was very unlike that of his native land. | Aquello era Transilvania, tal como la había descrito el conde Massen. El paisaje nada tenía que ver con el de su tierra. |
| N32 | The rays of the sun were blocked out by a dark cloud, surely an effect of the magic spell. As the dust cleared, the once-solid castle could be seen in ruins. | Una nube oscura tapó los rayos del sol, sin duda obra del hechizo. Al disiparse el polvo, el castillo, antes tan sólido, apareció en ruinas. |
| N33 | Social descended the mountain, and found a road leading to town. Jonathan Harker had given him directions so he could return to England safely. | Social bajó de la montaña y encontró un camino hacia el pueblo. Jonathan Harker le había explicado cómo volver a Inglaterra sano y salvo. |
| N34 | Thanks to Social, London was free from "the curse of the vampire," and people could at last walk at night... without fear. | Gracias a Social, Londres quedó libre de «la maldición del vampiro», y la gente pudo por fin pasear de noche... sin miedo. |
| N35 | Now we have electric lights to help us feel safe. It seems as if the horrors of the past will finally fade away. | Hoy tenemos luz eléctrica que nos hace sentir seguros. Parece que los horrores del pasado se desvanecerán al fin. |
| N36 | But we must never forget: Dracula's curse cannot be lifted. Dracula may return. The full moon still troubles the human heart... You must always be careful when walking at night... when the full moon shines. | Pero nunca debemos olvidar que la maldición de Drácula no se puede romper. Drácula puede volver. La luna llena aún inquieta al corazón humano... Ten siempre cuidado al caminar de noche... cuando brille la luna llena. |

---

## Cómo preparar las voces

- Un archivo por código, en **Ogg Vorbis** (`.ogg`) o **MP3** (si me los pasas en MP3 los convierto yo).
- Nombre que empiece por el código: `N05.ogg` o `N05 - Have you not heard.ogg`.
- Una carpeta por idioma: `ingles/` y `espanol/` (o dos zip). Si un código no tiene voz, esa página sale
  sin narración.
- Duración: las páginas de las rondas esperan a que pulses un botón (salvo en la introducción, donde el juego
  pasa de página solo). Si una voz de la introducción dura más que su página, la app intentará hacer esperar al
  juego; mejor que no pasen de unos 8–10 segundos.
- Sin silencio largo al principio ni al final, y todas a un volumen parecido (yo las igualo al montar el pack).
