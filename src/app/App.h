#pragma once
// O "cerebro" da interface fisica: transforma botoes e knob em acoes.
//
// Botoes:
//   Vermelho  abre a pergunta "Trocar de modo?"; apertar de novo cancela.
//   Verde     confirma a troca de modo / toca a faixa da lista / play-pause.
//   Azuis     abrem a lista de musicas e movem o cursor.
//             (segurar o da direita na lista tambem toca a faixa)
// Knob        volume.

namespace app {
void begin();    // configura os botoes e a tela inicial do modo atual
void update();   // chamar no loop: le botoes e knob
void render();   // chamar no loop: desenha a tela
}  // namespace app
