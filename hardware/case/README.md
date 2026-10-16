# Case BMO do Localfy (V1)

Case em formato de BMO (Hora de Aventura), **60 × 100 × 40 mm**, para ESP32
DevKit, OLED 0,96", módulo microSD, 4 botões, potenciômetro de volume e bateria
LiPo achatada.

- Modelo paramétrico: [bmo_case.scad](bmo_case.scad). Abra no OpenSCAD; `part = "montagem"` mostra tudo montado, com os componentes "fantasmas" dentro.
- STLs prontos: [stl/](stl/)

> **Meça seus componentes com paquímetro antes de imprimir** e ajuste as
> variáveis no topo do `.scad` (principalmente `esp_w`, `esp_l`, `oled_pcb`,
> `oled_aa_dy`, `sd_pcb`, `sd_x_card`, `tp_size`, `tact_h`, `pot_boss_t`).
> Já medidos: ESP32 53 × 28 mm, OLED 25 × 25 mm, microSD 20 × 20 mm (soquete +3 mm, cartão +5 mm),
> tact switch 12 × 12 × 7,3 mm; vidro do OLED dentro da placa. Pelas lojas/datasheet: TP4056 USB-C 26 × 17 mm, chave SS12D07 8,6 × 4,3 × 4,7 mm. Falta conferir: comprimento da rosca do potenciômetro (`pot_boss_t`, hoje 7 mm).
> Para gerar um STL de novo: `openscad -D 'part="frente"' -o stl/frente.stl bmo_case.scad`

## O que vai onde

| Elemento do BMO | Função |
|---|---|
| Visor | Moldura em relevo com o OLED 0,96" no centro (imprima em verde-claro ou branco para ficar com cara de tela do BMO) |
| Botão **vermelho** | Trocar de modo (Bluetooth ↔ Web) |
| Botão **verde** | Play/pause e "confirmar" |
| Tracinhos **azuis** | Voltar / passar (abrem a lista de músicas) |
| Fenda das fitas, cruz, triângulo, bolinha | Decorativos |
| Ombro esquerdo | Knob do **volume** (potenciômetro atravessando a lateral) |
| Laterais | "BMO" em preto, a versão "ev1" gravada pequena logo depois e os furinhos. Na direita lê de cima para baixo, na esquerda de baixo para cima (para trocar a versão: `version_tag`) |
| Base | Entrada do **cartão microSD** (com rebaixo para a unha) e **USB de carga** |
| Tampa | Chave liga/desliga, 4 parafusos M2 pelas laterais, o encaixe do **clipe de cinto** e os 4 furos do **suporte de celular de moto** |

## Organização interna

```
          vista de lado (frente à esquerda)         z = profundidade
 ┌──────┬──────────────────────────────────┬──┐
 │frente│ OLED / placa dos 4 botões        │  │
 │      │      ESP32 de pé (lateral dir.)  │T │  tampa: bateria LiPo (alto),
 │      │      pot. de volume (lat. esq.)  │A │         microSD e TP4056 (baixo),
 │      │      MT3608 em pé na base        │M │         chave liga/desliga
 └──────┴──────────────────────────────────┴──┘
```

- **Frente (z ≈ 2–12 mm):** OLED preso nas cantoneiras e **uma única perfboard
  (52 × 28 mm)** com os 4 tact switches, presa em 4 pinos.
- **Lateral direita:** ESP32 de pé, com o módulo metálico encostado na parede,
  o USB para baixo (dentro) e a **antena no alto, ao lado dos furinhos**.
  Ele se apoia em um batente embaixo e em dois batentes na frente.
- **Lateral esquerda:** potenciômetro preso pela porca, o eixo sai no "ombro".
- **Base:** regulador MT3608 em pé, entre as guias, com fita dupla face.
- **Tampa:** bateria LiPo num berço sobre um degrau de 4 mm (metade de cima), módulo microSD e
  carregador TP4056 deitados em molduras (metade de baixo), com o soquete e o
  USB virados para os furos da base.

Com as medidas típicas dos módulos, nenhum componente encosta na case nem em
outro componente (conferido por interseção no OpenSCAD em 05/10/2026). Se as
medidas reais forem diferentes, ajuste os parâmetros e confira na `montagem`.

## Como cada componente fica preso

| Componente | Fixação |
|---|---|
| Botões impressos | presos entre a parede e o switch pela aba |
| Placa dos 4 botões | 4 pinos com furo para parafuso M2 (ou cola quente) |
| Potenciômetro | porca apertada por fora da lateral |
| OLED | 4 cantoneiras + um ponto de cola quente em cada canto |
| ESP32 | batente embaixo, 2 batentes na frente e **2 nervuras da tampa** que encostam atrás quando ela fecha |
| Suporte de moto | 4 ganchos da placa do suporte nos furos das costas |
| MT3608 | **guias na parede de baixo** (de pé, entre um degrau e um trilho) + fita dupla face |
| Bateria | berço na tampa + fita dupla face |
| microSD e carregador | molduras na tampa (seguram o empurrão do cartão e do plugue) + um ponto de cola |
| Chave liga/desliga | **berço em volta do corpo** na tampa + um ponto de cola |

## Peças para imprimir

| STL | Cor sugerida | Qtd | Orientação |
|---|---|---|---|
| `frente.stl` | verde-água do BMO | 1 | frente na mesa (já está assim) |
| `tampa.stl` | verde-água | 1 | face externa na mesa (já está assim) |
| `moldura.stl` | verde-claro ou branco | 1 | plana, chanfro para cima |
| `letras.stl` | preto | **2 jogos** | plana; cole com a face de cima para fora |
| `fenda.stl` | preto | 1 | plana |
| `vermelho.stl` | vermelho | 1 | aba na mesa |
| `verde.stl` | verde | 1 | aba na mesa |
| `pilula.stl` | azul | **2** | aba na mesa |
| `teste_suporte.stl` | qualquer | 1 | plana; teste rápido do encaixe no suporte de moto |
| `knob.stl` (opcional: dá para usar um knob de guitarra) | preto | 1 | topo na mesa (já está assim) |
| `cruz.stl` / `triangulo.stl` / `bolinha.stl` | amarelo / azul-claro / azul | 1 cada | plana |

Sugestão: PLA, camada 0,2 mm (0,12 mm nas peças pequenas), 3 perímetros, 20% de
preenchimento, **sem suporte** (as bordas chanfradas e o friso já foram
pensados para imprimir sem suporte).

**Para economizar filamento nos testes:** imprima primeiro só uma peça pequena
(a bolinha) para acertar a folga (`tol = 0.15`), e depois a tampa, que é fina e
rápida, para conferir o encaixe da bateria, do SD e do TP4056. A frente
(a peça grande) vai por último.

## Clipe de cinto (de metal)

As costas têm um encaixe para um **clipe de metal de trena**, preso por um parafuso.

- **Encaixe em V:** um rebaixo de 1 mm com o formato da base do clipe (19 mm em
  cima, afinando até 16 mm, com 20 mm de altura). As paredes seguram as
  laterais, então o clipe não gira com um parafuso só. É rebaixo, e não
  relevo, porque a tampa imprime com a face de fora na mesa.
- **Parafuso:** o furo guia é **cego** (4 mm de profundidade) e não chega na
  bateria. Use parafuso de **~3 mm de diâmetro e até 5 mm de comprimento** (o da própria trena costuma servir, ou um M3 × 5): mais longo que isso pode furar o lado de dentro, onde fica a bateria.
  A bateria fica num degrau de 4 mm justamente para dar material para essa rosca.
- Se o seu clipe tiver outra medida, ajuste `clip_top_w`, `clip_bottom_w`,
  `clip_guide_h` e `clip_hole_y` (posição do furo) no início do `bmo_case.scad`.

## Suporte de celular de moto

As costas têm 4 furos para a placa de ganchos dos suportes de celular de moto
(aqueles da bolsa impermeável): placa de 45 × 33 mm com 4 ganchos de 3 × 3 mm
a 4 mm da borda e ~2,5 mm de altura. O BMO entra no lugar da bolsa.

- A placa fica **deitada**, centralizada na largura e o mais alto possível,
  parando 2 mm abaixo da ponta da lâmina do clipe de metal. Com o clipe montado,
  dá para prender na moto do mesmo jeito. A altura é calculada a partir de
  `clip_leaf_l`, o comprimento da lâmina medido do topo do V até a ponta (hoje
  está estimado em 45 mm). Testado sem colisão com lâminas de 25 a 55 mm.
- Os furos têm 3,3 mm. Por dentro, em volta de cada furo, a tampa é rebaixada
  até **1,4 mm** (`mount_grip`), a espessura que o dente do gancho abraça.
  O rebaixo fica acima e abaixo de cada furo, então funciona com o dente virado
  para qualquer um dos dois lados.
- A ponta dos ganchos entra ~0,5 mm no interior, por baixo do módulo microSD
  e do TP4056. Por isso o TP4056 fica 2 mm acima da tampa (`tp_standoff`).
- **Teste antes:** imprima o `teste_suporte.stl`, que é um pedaço da tampa com os
  4 furos e leva ~10 minutos, e encaixe na moto. Se ficar frouxo, diminua
  `mount_grip`. Se não entrar, aumente `mount_grip` ou `mount_clear`. O ideal é
  medir com o paquímetro a espessura da placa da bolsa original nos furos e usar
  esse valor.
- A chave liga/desliga continua acessível com o BMO no suporte.

## Base de mesa

[bmo_base.scad](bmo_base.scad) gera uma base (84 × 64 × 24 mm) onde o BMO
encaixa em pé, com "LOCALFY" gravado na frente. Ela lê as medidas da case,
então acompanha qualquer ajuste feito no `bmo_case.scad`.

- O BMO entra num rebaixo de 6 mm, abaixo do knob de volume, com 0,4 mm de folga por lado.
- **Dá para carregar com ele na base:** há um poço sob o USB de carga e um
  canal por baixo, por onde o cabo sai na parte de trás.
- Para trocar o cartão microSD (também na face de baixo), tire o BMO da base.
- Imprima com o fundo na mesa, sem suporte (`stl/base.stl`). Qualquer cor; branco ou cinza-claro destaca o BMO.
- `peca = "previa"` mostra a base com o BMO encaixado.

## Componentes

| Item | Observação |
|---|---|
| ESP32 DevKit (micro-USB) | De pé na lateral direita. Para regravar com tudo montado, abra a tampa. |
| OLED SSD1306 0,96" I2C | Pinos para cima |
| Módulo microSD pequeno (20 × 20 mm) | Deitado na tampa, soquete para baixo; o cartão fica rente à face de baixo |
| 4 × tact switch **12×12×7,3 mm** (pino quadrado) | Todos na mesma perfboard, centralizados sob os botões |
| Potenciômetro com **rosca e porca** e eixo serrilhado de 6 mm | Use um **knob de guitarra** (encaixa nesse eixo) ou o `knob.stl` |
| Bateria LiPo **103450** (34×50×10 mm) com proteção e plugue JST-PH | |
| Módulo **TP4056 com proteção, USB-C** (26 × 17 mm) | Carregador |
| Módulo **MT3608** | Elevador de tensão para 5 V |
| Chave deslizante **SS12D07** vertical (alavanca 4 mm) | Liga/desliga, colada por dentro da tampa |
| 4 × parafuso M2 × 6 mm autoatarraxante | Tampa |
| Clipe de metal de trena + parafuso ~3 mm × até 5 mm | Costas |

**Solde os fios direto nos pinos, sem jumpers dupont.** Com 40 mm de
profundidade, os conectores dupont não cabem.

## Alimentação

```
LiPo 3,7 V ──► TP4056 (B+/B-) ──OUT+──► chave ──► MT3608 IN ──OUT 5 V──► ESP32 pino 5V/VIN
                  ▲                                                    GND comum = OUT- do TP4056
               USB (carga, na base)
```

> ⚠️ **Regule o MT3608 para 5,0 V com o multímetro ANTES de ligar no ESP32.**
> Ele pode vir de fábrica ajustado para bem mais de 5 V.

- O GND do circuito sai do **OUT-** do TP4056, nunca do B-.
- Para gravar o firmware pelo micro-USB, **desligue a chave** antes.
- Mantenha a bateria longe da antena do ESP32 (alto da lateral direita).

## Ligações

| Componente | ESP32 |
|---|---|
| OLED SDA / SCL | GPIO 21 / 22 |
| SD CS / SCK / MISO / MOSI | GPIO 5 / 18 / 19 / 23 |
| Azul direito (próxima) | GPIO 32 → GND |
| Azul esquerdo (anterior) | GPIO 33 → GND |
| Verde (play/pause) | GPIO 27 → GND |
| Vermelho (modo) | GPIO 26 → GND |
| LED branco (modo Web) | GPIO 15 → resistor 47 Ω → LED → GND |
| Potenciômetro (pino do meio) | GPIO 35, pontas em 3V3 e GND |

Todos os botões usam o pull-up interno: basta ligar entre o pino e o GND, sem resistor.

## Montagem

1. Cole os enfeites, o visor e a fenda nos rebaixos da frente, e as letras "BMO" nas duas laterais.
2. Coloque os 4 botões pelos furos, por dentro, com a aba para trás. Cada botão
   impresso é só um pino que empurra o switch: o formato de fora (pílula, círculo) é visual.
3. Parafuse ou cole a perfboard dos botões nos 4 pinos.
4. Passe o potenciômetro por dentro da lateral esquerda, aperte a porca por fora e encaixe o knob.
5. Encaixe o OLED nas cantoneiras, com o vidro encostado na parede, e fixe com cola quente.
6. Encaixe o ESP32 de pé na lateral direita, apoiado no batente de baixo (as nervuras da tampa o travam quando ela fecha).
7. Deslize o MT3608 em pé entre as guias da base, já regulado em 5 V, com fita dupla face.
8. Na tampa: bateria no berço (fita dupla face), microSD e TP4056 nas molduras e a chave no berço, com a alavanca na fenda.
   Por fora, encaixe o clipe de metal no rebaixo em V e parafuse.
9. Feche e aperte os 4 parafusos M2 pelas laterais. Teste o cartão e o USB de carga pela base.
