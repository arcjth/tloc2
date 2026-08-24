Localização sonora bidimensional por TDOA com arranjo
de quatro microfones

Este trabalho apresenta o desenvolvimento de um sistema
de localização sonora bidimensional baseado na diferença
de tempo de chegada (TDOA - Time Difference of Arrival)
do som captado por um arranjo de quatro microfones.

Partindo da hipótese de que a origem espacial de um evento
sonoro pode ser inferida a partir das diferenças temporais
de chegada do sinal em cada microfone, o problema foi
formulado como um sistema de equações lineares Ax = b.

Os microfones foram posicionados segundo vetores unitários
ortogonais entre si: (1,0), (0,1), (-1,0) e (0,-1),
obtidos a partir de rotações sucessivas de 90° do vetor
de referência (equivalentes às multiplicações por i, i²
e i³ no plano complexo).

Tomando o microfone de referência (1,0) como base, as
diferenças de distância (r) entre os demais microfones e
a fonte sonora compõem o vetor b, permitindo resolver o
sistema por eliminação de Gauss com pivoteamento parcial
e obter a posição estimada (x, y) da fonte, além da
distância até o microfone de referência.

A implementação evoluiu em duas etapas de hardware.

Na primeira, os quatro microfones foram lidos manualmente
via protocolo I2S por software em um Arduino Nano; essa
abordagem se mostrou insuficiente em taxa de amostragem
e sincronismo entre canais, inviabilizando a captura
simultânea exigida pelo método.

Na segunda etapa, o projeto migrou para um ESP32, que
dispõe de periféricos I2S dedicados em hardware: os quatro
microfones foram distribuídos em dois barramentos I2S
estéreo (dois microfones por barramento, nos canais L e R),
com o segundo barramento configurado em modo escravo,
sincronizado ao clock do primeiro (mestre), garantindo a
leitura simultânea dos quatro canais sobre a mesma base
de tempo.

A detecção de eventos sonoros é feita por limiar de energia
(RMS) sobre os quatro canais.

Uma vez detectado um evento, um filtro casado (matched
filter, via correlação cruzada) é aplicado entre o canal de referência e cada
um dos demais canais para estimar o atraso (lag) entre eles.

Esse atraso é convertido na diferença de distância (r)
correspondente, que alimenta o sistema linear descrito
acima, resultando na posição estimada da fonte sonora.
