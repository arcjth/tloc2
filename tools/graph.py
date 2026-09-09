import threading
import queue
import time
import random
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

X_MIN, X_MAX = 0.0, 90.0
Y_MIN, Y_MAX = 0.0, 60.0

DISTANCIA_ENTRE_MICROFONES = 30.0

POSICOES_MICROFONES = [
    (i * DISTANCIA_ENTRE_MICROFONES, 0.0) for i in range(4)
]

TAMANHO_GRADE = 100
SIGMA_GAUSSIANA = 3.0
TAXA_DECAIMENTO = 0.95
INTERVALO_SINAL = 0.05
INTERVALO_PLOT_MS = 100
PASSO_CAMINHADA = 1.5


class SimuladorDeSinal(threading.Thread):
    def __init__(self, fila_dados: queue.Queue):
        super().__init__(daemon=True)
        self.fila_dados = fila_dados
        self.ativo = True
        self.pos_x = (X_MIN + X_MAX) / 2
        self.pos_y = (Y_MIN + Y_MAX) / 2

    def run(self):
        while self.ativo:
            passo_x = random.uniform(-PASSO_CAMINHADA, PASSO_CAMINHADA)
            passo_y = random.uniform(-PASSO_CAMINHADA, PASSO_CAMINHADA)

            self.pos_x = min(max(self.pos_x + passo_x, X_MIN), X_MAX)
            self.pos_y = min(max(self.pos_y + passo_y, Y_MIN), Y_MAX)

            self.fila_dados.put((self.pos_x, self.pos_y))
            time.sleep(INTERVALO_SINAL)

    def parar(self):
        self.ativo = False


class VisualizadorMapaDeCalor:
    def __init__(self, fila_dados: queue.Queue):
        self.fila_dados = fila_dados
        self.mapa = np.zeros((TAMANHO_GRADE, TAMANHO_GRADE), dtype=np.float64)

        self.figura, self.eixo = plt.subplots(figsize=(7, 6))
        self.imagem = self.eixo.imshow(
            self.mapa,
            cmap="jet",
            origin="lower",
            extent=[X_MIN, X_MAX, Y_MIN, Y_MAX],
            vmin=0,
            vmax=1,
            interpolation="bilinear",
        )
        self.eixo.set_title("Mapa de calor em tempo real")
        self.eixo.set_xlabel("X")
        self.eixo.set_ylabel("Y")
        self.figura.colorbar(self.imagem, ax=self.eixo, label="Intensidade")

        self.ponto_atual, = self.eixo.plot([], [], "o", color="cyan", markersize=8)

    def _converter_para_grade(self, x, y):
        col = int((x - X_MIN) / (X_MAX - X_MIN) * (TAMANHO_GRADE - 1))
        lin = int((y - Y_MIN) / (Y_MAX - Y_MIN) * (TAMANHO_GRADE - 1))
        return col, lin

    def _depositar_intensidade(self, x, y):
        col, lin = self._converter_para_grade(x, y)
        raio = int(SIGMA_GAUSSIANA * 3)

        col_ini, col_fim = max(0, col - raio), min(TAMANHO_GRADE, col + raio + 1)
        lin_ini, lin_fim = max(0, lin - raio), min(TAMANHO_GRADE, lin + raio + 1)

        colunas = np.arange(col_ini, col_fim)
        linhas = np.arange(lin_ini, lin_fim)
        grade_col, grade_lin = np.meshgrid(colunas, linhas)

        gaussiana = np.exp(
            -(((grade_col - col) ** 2 + (grade_lin - lin) ** 2) / (2 * SIGMA_GAUSSIANA ** 2))
        )
        self.mapa[lin_ini:lin_fim, col_ini:col_fim] += gaussiana

    def atualizar(self, frame):
        ultimo_ponto = None

        while not self.fila_dados.empty():
            x, y = self.fila_dados.get_nowait()
            self._depositar_intensidade(x, y)
            ultimo_ponto = (x, y)

        self.mapa *= TAXA_DECAIMENTO

        valor_maximo = self.mapa.max()
        mapa_normalizado = self.mapa / valor_maximo if valor_maximo > 0 else self.mapa
        self.imagem.set_data(mapa_normalizado)

        if ultimo_ponto is not None:
            self.ponto_atual.set_data([ultimo_ponto[0]], [ultimo_ponto[1]])

        return self.imagem, self.ponto_atual

    def executar(self):
        anim = animation.FuncAnimation(
            self.figura,
            self.atualizar,
            interval=INTERVALO_PLOT_MS,
            blit=False,
            cache_frame_data=False,
        )
        plt.show()


def main():
    fila_dados = queue.Queue()

    simulador = SimuladorDeSinal(fila_dados)
    simulador.start()

    visualizador = VisualizadorMapaDeCalor(fila_dados)
    try:
        visualizador.executar()
    finally:
        simulador.parar()


if __name__ == "__main__":
    main()