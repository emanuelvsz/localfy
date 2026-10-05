"""Gera o esquemático da placa-mãe do Localfy (esquematico.svg).

Uso:  pip install schemdraw && python gerar_esquematico.py

As ligações usam rótulos de rede (net labels): pinos com o mesmo nome
estão ligados entre si. A lista completa está em LIGACOES.md; se mudar
algo aqui, mude lá também.
"""
import schemdraw
import schemdraw.elements as elm

SIDES = {"left": 180, "right": 0, "top": 90, "bottom": 270}


def module(d, xy, title, left=(), right=(), w=None):
    """Bloco com pinos; cada pino recebe o nome da rede na ponta do terminal."""
    pins = []
    for side, lst in (("left", left), ("right", right)):
        # o Ic numera os pinos de baixo para cima; invertemos para ler de cima
        for i, (name, _net) in reversed(list(enumerate(lst))):
            pins.append(elm.IcPin(name=name, side=side, anchorname=f"{side[0]}{i}"))
    ic = elm.Ic(pins=pins, pinspacing=1.0, leadlen=1.0, size=(w, max(len(left), len(right)) + 0.5) if w else None)
    d.add(ic.at(xy).theta(0).label(title, "top", fontsize=13))
    for side, lst in (("left", left), ("right", right)):
        for i, (_name, net) in enumerate(lst):
            a = getattr(ic, f"{side[0]}{i}")
            ha = "right" if side == "left" else "left"
            dx = -0.2 if side == "left" else 0.2
            d.add(elm.Label().at((a[0] + dx, a[1])).label(net, halign=ha, color="#b5179e", fontsize=11))
    return ic


def net(d, xy, name, ha="center", va="bottom"):
    d.add(elm.Label().at(xy).label(name, halign=ha, valign=va, color="#b5179e", fontsize=11))


with schemdraw.Drawing(file="esquematico.svg", show=False) as d:
    d.config(fontsize=11, unit=2)

    d.add(elm.Label().at((-30, 20)).label(
        "Localfy — placa-mãe (opção A: ESP32 DevKit + módulos)", halign="left", fontsize=16))
    d.add(elm.Label().at((-30, 19)).label(
        "Pinos com o mesmo nome em rosa estão ligados entre si. GND é comum a tudo (usar OUT- do TP4056, nunca B-).",
        halign="left", fontsize=10))

    # ---------------- alimentação ----------------
    module(d, (-30, 6), "J1  Bateria LiPo 1S (JST-PH 2 vias)",
           right=[("1 (+)", "VBAT+"), ("2 (-)", "VBAT-")])
    module(d, (-19, 6), "U2  TP4056 c/ proteção (micro-USB ou USB-C)",
           left=[("B+", "VBAT+"), ("B-", "VBAT-")],
           right=[("OUT+", "VSYS"), ("OUT-", "GND")])
    d.add(elm.Label().at((-17.5, 2.2)).label("(o USB de carga fica no próprio módulo)", fontsize=9))
    module(d, (-30, -2), "SW1  Chave liga/desliga",
           right=[("COM", "VSYS"), ("ON", "VSW"), ("OFF", "—")])
    module(d, (-19, -2), "U3  MT3608 (ajustar 5,0 V!)",
           left=[("VIN+", "VSW"), ("VIN-", "GND")],
           right=[("VOUT+", "+5V"), ("VOUT-", "GND")])

    # capacitor de reserva no 5 V
    d.add(elm.Line().at((-29, -9)).up(1))
    net(d, (-29, -8), "+5V")
    d.add(c1 := elm.Capacitor(polar=True).at((-29, -9)).down().label("C1\n100 µF 10 V", loc="bottom", ofst=0.3))
    d.add(elm.Ground().at(c1.end))

    # divisor para medir a bateria
    d.add(elm.Line().at((-22, -8)).down(0.5))
    net(d, (-22, -8), "VSW")
    d.add(r1 := elm.Resistor().at((-22, -8.5)).down().label("R1 100k", loc="bottom"))
    d.add(elm.Dot().at(r1.end))
    d.add(elm.Line().at(r1.end).right(1.5))
    net(d, (-20.4, r1.end[1]), "VBAT_SENSE", ha="left", va="center")
    d.add(r2 := elm.Resistor().at(r1.end).down().label("R2 100k", loc="bottom"))
    d.add(elm.Ground().at(r2.end))
    d.add(elm.Line().at(r1.end).left(2))
    d.add(c3 := elm.Capacitor().at((-24, r1.end[1])).down().label("C3 100n", loc="top"))
    d.add(elm.Ground().at(c3.end))

    # ---------------- ESP32 ----------------
    module(d, (-4, 8), "U1  ESP32 DevKit (em barras fêmea)",
           left=[("VIN / 5V", "+5V"), ("GND", "GND"), ("3V3", "+3V3")],
           right=[("GPIO21", "OLED_SDA"), ("GPIO22", "OLED_SCL"),
                  ("GPIO5", "SD_CS"), ("GPIO18", "SD_SCK"), ("GPIO19", "SD_MISO"), ("GPIO23", "SD_MOSI"),
                  ("GPIO32", "BTN_NEXT"), ("GPIO33", "BTN_PREV"), ("GPIO27", "BTN_PLAY"),
                  ("GPIO26", "BTN_MODO"), ("GPIO35", "VOL"), ("GPIO36 (VP)", "VBAT_SENSE"),
                  ("GPIO15", "LED_WIFI")], w=5)

    # ---------------- periféricos ----------------
    module(d, (12, 8), "U4  OLED SSD1306 0,96\" I2C",
           left=[("GND", "GND"), ("VCC", "+3V3"), ("SCL", "OLED_SCL"), ("SDA", "OLED_SDA")])
    module(d, (12, 0), "J2  Cabo p/ módulo microSD (6 vias)",
           left=[("GND", "GND"), ("VCC", "+5V"), ("MISO", "SD_MISO"),
                 ("MOSI", "SD_MOSI"), ("SCK", "SD_SCK"), ("CS", "SD_CS")])

    # botões: GPIO -> botão -> GND (pull-up interno do ESP32)
    for i, (ref, name, label) in enumerate([("SW2", "BTN_PREV", "voltar"),
                                             ("SW3", "BTN_NEXT", "passar"),
                                             ("SW4", "BTN_PLAY", "play/pause (verde)")]):
        x = -4 + i * 4
        d.add(elm.Line().at((x, -6)).down(0.5))
        net(d, (x, -6), name)
        d.add(b := elm.Button().at((x, -6.5)).down().label(f"{ref}\n{label}", loc="bottom", ofst=0.2))
        d.add(elm.Ground().at(b.end))

    # botão de modo (vermelho): pull-up interno do GPIO26, igual aos outros botões
    d.add(elm.Line().at((8, -6)).down(0.5))
    net(d, (8, -6), "BTN_MODO")
    d.add(b := elm.Button().at((8, -6.5)).down().label("SW5\nmodo (vermelho)", loc="bottom", ofst=0.2))
    d.add(elm.Ground().at(b.end))

    # LED do modo Wi-Fi
    d.add(elm.Line().at((22, -5)).down(0.5))
    net(d, (22, -5), "LED_WIFI")
    d.add(r4 := elm.Resistor().at((22, -5.5)).down().label("R4 47R", loc="bottom"))
    d.add(led := elm.LED().at(r4.end).down().label("D1\nbranco", loc="bottom"))
    d.add(elm.Ground().at(led.end))

    # potenciômetro de volume
    d.add(elm.Line().at((14, -5)).down(0.5))
    net(d, (14, -5), "+3V3")
    d.add(p := elm.Potentiometer().at((14, -5.5)).down().label("RV1\n10k lin.", loc="top", ofst=0.3))
    d.add(elm.Ground().at(p.end))
    d.add(elm.Line().at(p.tap).right(1.5))
    d.add(elm.Dot())
    wiper = (p.tap[0] + 1.5, p.tap[1])
    net(d, (wiper[0] + 0.2, wiper[1]), "VOL", ha="left", va="center")
    d.add(c2 := elm.Capacitor().at(wiper).down().label("C2 100n", loc="bottom"))
    d.add(elm.Ground().at(c2.end))
