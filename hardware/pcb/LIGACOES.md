# Placa-mãe do Localfy: lista de ligações

Opção A: placa portadora para o **ESP32 DevKit** e os módulos prontos, só com
componentes PTH (fácil de soldar à mão). Esquemático: [esquematico.svg](esquematico.svg)
(gerado por [gerar_esquematico.py](gerar_esquematico.py)).

## Redes (o que liga com o quê)

Cada linha é uma ligação elétrica. Tudo na mesma linha fica ligado entre si.

### Alimentação

| Rede | Pontos ligados | Observação |
|---|---|---|
| `VBAT+` | J1.1 (bateria +) · U2.B+ | |
| `VBAT-` | J1.2 (bateria −) · U2.B- | **Não** é o GND do circuito |
| `VSYS` | U2.OUT+ · SW1.COM | Bateria protegida, antes da chave |
| `VSW` | SW1.ON · U3.VIN+ · R1 (topo) | Só tem tensão com a chave ligada |
| `+5V` | U3.VOUT+ · U1.VIN/5V · J2.VCC · C1 (+) | |
| `+3V3` | U1.3V3 · U4.VCC · RV1 (ponta alta) | Sai do regulador do próprio DevKit |
| `GND` | U2.OUT- · U3.VIN- · U3.VOUT- · U1.GND · U4.GND · J2.GND · RV1 (ponta baixa) · SW2/SW3/SW4/SW5 · D1 (catodo) · R2 · C1 (−) · C2 · C3 | Plano de terra |

> ⚠️ **O GND do circuito é o `OUT-` do TP4056, nunca o `B-`.** No módulo com
> proteção, o `B-` passa pelos MOSFETs de proteção. Ligar a carga no `B-` anula
> a proteção contra descarga total da bateria.

### Sinais

| Rede | ESP32 (U1) | Vai para | Observação |
|---|---|---|---|
| `OLED_SDA` | GPIO21 | U4.SDA | O módulo OLED já tem pull-up |
| `OLED_SCL` | GPIO22 | U4.SCL | |
| `SD_CS` | GPIO5 | J2.CS | |
| `SD_SCK` | GPIO18 | J2.SCK | |
| `SD_MISO` | GPIO19 | J2.MISO | |
| `SD_MOSI` | GPIO23 | J2.MOSI | |
| `BTN_NEXT` | GPIO32 | SW3 → GND | Pull-up interno |
| `BTN_PREV` | GPIO33 | SW2 → GND | Pull-up interno |
| `BTN_PLAY` | GPIO27 | SW4 (verde) → GND | Pull-up interno · play/pause e confirmar |
| `BTN_MODO` | GPIO26 | SW5 (vermelho) → GND | Pull-up interno · abre a troca de modo (confirmar no verde) |
| `VOL` | GPIO35 | cursor do RV1 · C2 → GND | ADC1 · volume |
| `VBAT_SENSE` | GPIO36 (VP) | meio de R1/R2 · C3 → GND | ADC1, metade da tensão da bateria · **firmware ainda não usa** |
| `LED_WIFI` | GPIO15 | R4 47 Ω → D1 (LED branco) → GND | Aceso só no modo Wi-Fi |

Os pinos 35 e 36 são só de entrada, **sem pull-up/pull-down interno**, e ficam
no **ADC1**. O ADC2 não funciona com o Wi-Fi ligado.
Os GPIO5 e GPIO15 são pinos de boot (strapping), mas os dois funcionam aqui: o
pull-up do módulo SD mantém o GPIO5 no nível certo, e o LED no GPIO15 no máximo
silencia o log da ROM na serial durante o boot.

## Lista de componentes

| Ref | Componente | Valor / modelo | Encapsulamento |
|---|---|---|---|
| U1 | ESP32 DevKit | o seu (micro-USB) | 2 barras fêmea 2,54 mm |
| U2 | Carregador | Módulo TP4056 **com proteção** (6 pads), micro-USB ou USB-C | Pads do módulo / pinos |
| U3 | Elevador de tensão | Módulo MT3608 | 4 pinos 2,54 mm |
| U4 | Display | OLED SSD1306 0,96" I2C | 4 pinos 2,54 mm |
| J1 | Bateria | JST-PH 2 vias | PTH |
| J2 | Cabo do módulo microSD | Conector 6 vias (JST-PH ou barra 2,54) | PTH |
| SW1 | Liga/desliga | Chave deslizante 3 pinos, ângulo reto | PTH |
| SW2–SW4 | Botões de música | Tact switch 6×6 mm, 5 mm de altura | PTH |
| RV1 | Volume | Potenciômetro 9 mm vertical (RK09), 10k linear (B10K), eixo de 15–20 mm | PTH |
| R1, R2 | Divisor da bateria | 100 kΩ 1% | PTH |
| R4 | Resistor do LED | 47 Ω | PTH |
| D1 | LED do modo Wi-Fi | LED branco 5 mm (ou 3 mm) | PTH |
| SW5 | Botão de modo (vermelho) | Tact switch 6×6 mm | PTH |
| C1 | Reserva do 5 V | 100 µF 10 V eletrolítico | PTH |
| C2, C3 | Filtro dos ADCs | 100 nF cerâmico | PTH |
| BT1 | Bateria | LiPo 1S 2000 mAh (103450) com plugue JST-PH | — |

## Regras para o desenho da PCB

1. **Antena do ESP32:** sem cobre (nem plano de terra) embaixo nem na frente da
   antena. O ideal é a ponta da antena sair para fora da borda da placa.
2. **Plano de GND** na camada de baixo, cobrindo a placa (exceto na região da antena).
3. **Trilhas de potência** (`VBAT`, `VSYS`, `VSW`, `+5V`, `GND`): 0,8 mm ou mais.
   Trilhas de sinal: 0,25–0,3 mm.
4. **C1** perto do pino VIN do ESP32. **C2 e C3** perto dos pinos 35 e 36.
5. **MT3608 longe do ESP32 e da antena** (é uma fonte chaveada e gera ruído).
6. As posições dos botões, do potenciômetro, do OLED, da fenda e dos furos de fixação
   vêm do [bmo_case.scad](../case/bmo_case.scad), para encaixar na case.

## Antes de mandar fabricar

Este esquemático é uma base bem montada, mas **não substitui o teste na bancada**.
Confira estes pontos:

- [ ] **Pinagem do seu DevKit.** Você disse que ele tem 28 pinos, e o comum é 30 ou 38.
      Confira a posição de VIN, GND, 3V3 e dos GPIOs usados na serigrafia da placa e
      meça a distância entre as duas fileiras. O footprint precisa bater exatamente.
- [ ] **Ordem dos pinos dos módulos** (TP4056, MT3608, OLED, SD). Ela muda entre
      fabricantes: copie a ordem dos **seus** módulos.
- [ ] **VCC do módulo SD.** O módulo azul comum tem regulador de 3,3 V e precisa
      de 5 V no VCC (por isso está no `+5V`). Se o seu não tiver regulador (sem o
      componente de 3 pernas), ligue no `+3V3`.
- [ ] **Monte tudo na protoboard primeiro**, incluindo os 3 itens novos
      (play/pause, volume, medidor de bateria), e teste com o firmware.
- [ ] Faça a **verificação de regras elétricas (ERC)** no esquemático do KiCad e a de
      **regras de desenho (DRC)** na placa antes de gerar os Gerbers.
