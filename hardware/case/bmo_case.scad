// =====================================================================
//  Case do Localfy no formato do BMO (Hora de Aventura) - 60 x 100 x 40 mm
// =====================================================================
//  Pecas (escolha em `part` ou exporte pela linha de comando com
//  openscad -D 'part="frente"' -o stl/frente.stl bmo_case.scad):
//
//    "montagem"  - visualizacao com os componentes fantasmas (nao imprime)
//    "frente"    - frente + 4 paredes (imprimir com a frente na mesa)
//    "tampa"     - tampa traseira (ja virada: face externa na mesa)
//    "moldura"   - visor em relevo (verde-claro ou branco fica mais BMO)
//    "letras"    - letras "BMO" das laterais, imprimir 2 jogos (preto)
//    "fenda"     - fenda das fitas, so decorativa (preto)
//    "vermelho"  - botao de trocar de modo (vermelho)
//    "verde"     - botao play/pause (verde)
//    "pilula"    - botoes anterior/proxima, imprimir 2 (azul)
//    "knob"      - knob do volume, no ombro esquerdo (preto)
//    "cruz", "triangulo", "bolinha" - enfeites da frente
//
//  Sistema de coordenadas (antes do espelhamento final, ver oriented()):
//  x = largura, y = altura (y=0 e a base), z = profundidade (z=0 e a face
//  da frente, z=D a face de tras). x cresce para a DIREITA de quem olha a
//  frente do BMO.
//
//  Organizacao interna (de frente para tras):
//    camada da frente  OLED + placa unica com os 4 botoes (z ~2..12)
//    lateral direita   ESP32 de pe, antena no alto, perto dos furinhos
//    lateral esquerda  potenciometro de volume (o "ombro" do BMO)
//    base              regulador MT3608 de pe
//    tampa             bateria LiPo (num degrau), modulo microSD e TP4056
//    costas            encaixe em V para um clipe de metal (de trena), 1 parafuso cego
//  O cartao microSD e o USB de carga ficam na face de baixo.
//
//  MECA SEUS COMPONENTES COM PAQUIMETRO e ajuste os parametros abaixo.
// =====================================================================

part = "montagem";

$fn = 48;

// ---------------- caixa ----------------
W = 60;             // largura
H = 100;            // altura
D = 40;             // profundidade total (frente + tampa)
wall = 2;           // paredes laterais
front_t = 2;        // espessura da frente
lid_t = 2;          // espessura da tampa
r_corner = 4;       // raio dos cantos vistos de frente
edge_chamfer = 1;   // chanfro nas bordas da frente e de tras
shell_D = D - lid_t;
inner_back = shell_D;   // face interna da tampa (z)
tol = 0.15;         // folga por lado das pecas encaixadas

// Friso decorativo em volta da caixa, logo atras da frente: separa a "face"
// do corpo, como na figura do BMO.
groove_z = 4;
groove_w = 0.8;
groove_d = 0.5;

// encaixe da tampa: aba interna + parafusos M2 pelas laterais
lip_t = 1.6;
lip_h = 4;
lip_gap = 0.2;
screw_y = [25, 75];
screw_z = shell_D - lip_h / 2;
screw_d_wall = 2.4;    // furo passante na parede
screw_d_pilot = 1.7;   // furo guia (M2 autoatarraxante)
boss_w = 3;
boss_l = 8;

// ---------------- botoes (caps + tact switch 12x12) ----------------
cap_protrude = 1.5;    // quanto o botao sai para fora da frente
flange_t = 1.0;        // aba que impede o botao de cair para fora
flange_extra = 1.25;   // aba maior que o furo, por lado
cap_clear = 0.25;      // folga entre botao e furo, por lado
cap_gap = 0.2;         // folga entre aba e o tact switch
tact_size = 12;        // tact switch 12 x 12 mm (medido), pino quadrado
tact_h = 7.3;          // altura total do tact switch (placa ate a ponta), medido
board_z = front_t + flange_t + cap_gap + tact_h;   // face da perfboard (z)
board_t = 1.6;
post_d = 3.6;
post_pilot = 1.6;

// ---------------- tela OLED 0,96" ----------------
scr_c = [30, 73];        // centro do visor do BMO
scr_size = [44, 38];
scr_r = 5;
inlay_pocket = 1.0;      // profundidade dos rebaixos dos enfeites
inlay_t = 1.4;           // espessura dos enfeites (fica 0,4 mm saltado)
frame_raise = 2.0;       // quanto o visor sobressai da frente
frame_chamfer = 0.6;
oled_aa = [21.74, 10.86];// area ativa (pixels)
oled_win_margin = 0.4;
wall_win = [26, 15];     // janela atravessando a parede
oled_pcb = [25, 25];     // medido
oled_aa_dy = 1.1;        // centro dos pixels acima do centro da placa (pinos no topo; estimado)
oled_rib_h = 4;

// ---------------- frente: botoes e enfeites ----------------
// Tudo fica dentro da largura do visor (x de 8 a 52 mm), como o "padding" do
// rosto do BMO no desenho.
slot_c = [21.5, 40.5];   // fenda das fitas (decorativa)
slot_size = [25, 5.5];
red_c = [39, 14];        // vermelho: trocar de modo
red_d = 10;
green_c = [47.5, 27];    // verde: play/pause
green_d = 7;
pill_cs = [[12, 13], [25, 13]];     // azuis: anterior / proxima (13 mm: cabem 2 switches de 12 mm)
pill_size = [8, 3.2];
dpad_c = [15.5, 28];
dpad_s = 10.5;
dpad_arm = 3.6;
tri_c = [36, 31];
tri_s = 5.5;
dot_c = [45, 40];
dot_d = 5;

// Placa unica de botoes (perfboard) atras da frente, com os 4 tact switches
// centralizados sob os botoes. Pinos de fixacao nos espacos livres entre eles.
btn_board = [4, 5, 52, 29];   // x, y, largura, altura
btn_posts = [[8, 28], [32, 26], [52, 13], [37, 29]];

// ---------------- laterais: nome, furinhos e ombro ----------------
// As letras leem de cima para baixo na lateral direita e de baixo para cima
// na esquerda, sempre com o topo das letras virado para tras, como no desenho.
name_font = "Liberation Sans:style=Bold";
name_size = [48, 19];    // comprimento total x altura das letras
name_c = [61, D / 2];    // centro (y, z)
name_pocket = 1.0;
name_t = 1.6;            // letras pretas ficam 0,6 mm saltadas
dots_c = [91, D / 2];    // furinhos (y, z), perto da antena do ESP32
dots_d = 2.4;

// Potenciometro 16 mm com rosca M7, atravessando a lateral esquerda.
pot_c = [28, 22.5];      // (y, z) do eixo
pot_body_d = 16;
pot_boss_t = 7;          // = comprimento da rosca do pot
pot_body_l = 9;
pot_hole_d = 7.4;
nut_cb_d = 12.6;         // rebaixo para a porca (10 mm entre faces)
nut_cb_h = 2.6;
knob_d = 13;
knob_h = 5;
shaft_d = 6.0;           // eixo recartilhado de 6 mm (aperto)
shaft_hole_h = 4.5;

// ---------------- ESP32 DevKit (de pe na lateral direita) ----------------
esp_w = 28;              // largura da placa (vira profundidade na case), medido
esp_l = 53;              // comprimento, medido
esp_pcb_t = 1.6;
esp_mod_t = 3.2;         // modulo metalico, encostado na parede
esp_y0 = 42.5;           // USB para baixo (dentro), antena no alto
esp_z0 = 5.5;
esp_pcb_x = W - wall - esp_mod_t - esp_pcb_t;

// ---------------- tampa: bateria, microSD e carregador ----------------
batt = [34, 50, 10];     // LiPo 103450
batt_pos = [7, 45.5];
bay_wall = 1.2;
bay_h = 4;
// Degrau sob a bateria: da 6 mm de material (tampa + degrau) para o parafuso
// do clipe entrar sem atravessar ate a bateria.
batt_platform = 4;

// Modulo microSD pequeno (20 x 20 mm) deitado na tampa, soquete para a base.
// O soquete passa 3 mm da borda da placa e o cartao inserido passa mais 5 mm
// do soquete. A placa fica recuada para o cartao terminar rente a face de
// baixo: o BMO continua parando em pe numa mesa.
sd_pcb = [20, 20];       // medido (so a placa)
sd_socket_out = 3;       // soquete alem da borda da placa (medido: 23 - 20)
sd_card_out = 5;         // cartao inserido alem do soquete (medido: 28 - 23)
sd_socket_y = sd_card_out;               // borda do soquete (y): cartao rente em y=0
sd_pos = [9, sd_socket_y + sd_socket_out];   // canto da placa (x, y)
sd_standoff = 1.5;
sd_pcb_t = 1.6;
sd_card_above_pcb = 1.0;
sd_x_card = sd_pos[0] + sd_pcb[0] / 2;   // centro do soquete (confira no seu modulo)
sd_card_z = inner_back - sd_standoff - sd_pcb_t - sd_card_above_pcb;
card_hole = [13, 2.6];
card_recess = [17, 7, 1.5]; // rebaixo externo para pegar o cartao com a unha

// Carregador TP4056 (micro-USB) deitado na tampa, USB virado para a base.
tp_size = [19, 25];
tp_pos = [33.5, wall];
tp_standoff = 1;
tp_port_z = inner_back - tp_standoff - 1.6 - 1.4;
tp_port_x = tp_pos[0] + tp_size[0] / 2;
tp_usb_hole = [11, 7];

// Regulador MT3608 de pe, colado na parede de baixo, entre a placa dos botoes e a tampa.
mt_box = [19, wall, 15, 37, 17, 6];   // x, y, z, comprimento, largura (y), altura (z)

sw_c = [53, 36];         // chave liga/desliga SS12D00 na tampa
sw_slot = [2.4, 6];

// ---------------- clipe de cinto de metal (costas) ----------------
// Encaixe para um clipe de metal como o de trena: um rebaixo em V com o
// formato da base do clipe (as paredes seguram as laterais, entao ele nao
// gira) e um furo cego para o parafuso. E rebaixo, e nao relevo, porque a
// tampa imprime com a face de fora na mesa.
clip_c = [W / 2, 81];    // centro do encaixe (x, y), atras da bateria
clip_top_w = 19;         // largura do clipe no topo
clip_bottom_w = 16;      // largura no fim do encaixe (afina em V)
clip_guide_h = 20;       // altura do encaixe
clip_recess = 1.0;       // profundidade (= espessura aproximada do clipe)
clip_clear = 0.3;        // folga por lado
clip_hole_y = 0;         // furo em relacao ao centro do encaixe (+ = para cima)
clip_pilot_d = 2.5;      // furo guia (parafuso M3 autoatarraxante ou o da trena)
clip_pilot_depth = 4;    // cego: sobra 1 mm ate o lado de dentro (degrau da bateria)

// =====================================================================
//  utilidades 2D
// =====================================================================
module rrect(size, r) {
    translate([r, r]) offset(r) square([size[0] - 2 * r, size[1] - 2 * r]);
}
module rrect_c(size, r) { translate(-size / 2) rrect(size, r); }
module stadium(size) {
    hull() for (s = [-1, 1]) translate([s * (size[0] - size[1]) / 2, 0]) circle(d = size[1]);
}
module outline() { rrect([W, H], r_corner); }
module inner_outline(extra = 0) { offset(-(wall + extra)) outline(); }

module name2d() {
    resize(name_size) text("BMO", size = 10, font = name_font, halign = "center", valign = "center");
}
module dots2d() {
    for (o = [[4, -2.5], [4, 2.5], [0, -5], [0, 0], [0, 5], [-4, -2.5], [-4, 2.5]])
        translate(o) circle(d = dots_d);
}

// Coloca um desenho 2D (x = sentido de leitura, y = topo das letras) na face
// externa de uma lateral; z local aponta para fora da caixa.
// side 0 = lateral direita do BMO, side 1 = esquerda. Escrito nas
// coordenadas finais, por isso desfaz o espelhamento de oriented().
module on_side(side, c) {
    oriented()
        if (side == 0)
            translate([0, c[0], c[1]]) multmatrix([[0, 0, -1, 0], [-1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]]) children();
        else
            translate([W, c[0], c[1]]) multmatrix([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]]) children();
}

module dpad2d(s, a) { square([s, a], center = true); square([a, s], center = true); }
module tri2d(s) { circle(d = s * 2 / sqrt(3) * 1.15, $fn = 3); }

// formas dos rebaixos da frente
module screen2d() { translate(scr_c) rrect_c(scr_size, scr_r); }
module slot2d()   { translate(slot_c) rrect_c(slot_size, 1.5); }
module dpad_pos() { translate(dpad_c) dpad2d(dpad_s, dpad_arm); }
module tri_pos()  { translate(tri_c) rotate(90) tri2d(tri_s); }
module dot_pos()  { translate(dot_c) circle(d = dot_d); }

module round_hole2d(c, d) { translate(c) circle(d = d + 2 * cap_clear); }
module pill_hole2d(c) { translate(c) stadium(pill_size + [2, 2] * cap_clear); }

// Prisma com o contorno do BMO e as bordas chanfradas na frente e/ou atras.
module chamfered_body(h, ch_front, ch_back) {
    hull() {
        linear_extrude(0.01) offset(-ch_front) outline();
        translate([0, 0, ch_front]) linear_extrude(h - ch_front - ch_back) outline();
        translate([0, 0, h - 0.01]) linear_extrude(0.01) offset(-ch_back) outline();
    }
}

// =====================================================================
//  frente + paredes
// =====================================================================
module front_shell() {
    difference() {
        union() {
            difference() {
                chamfered_body(shell_D, edge_chamfer, 0);
                translate([0, 0, front_t]) linear_extrude(shell_D) inner_outline();
            }
            oled_ribs();
            esp32_supports();
            pot_boss();
            for (p = btn_posts)
                translate([p[0], p[1], 0]) cylinder(d = post_d, h = board_z);
        }

        // friso decorativo
        translate([0, 0, groove_z]) linear_extrude(groove_w) difference() {
            offset(1) outline();
            offset(-groove_d) outline();
        }

        // visor: janela do OLED e rebaixo da moldura
        translate([scr_c[0], scr_c[1], -1]) linear_extrude(front_t + 2) square(wall_win, center = true);
        pocket() screen2d();

        // enfeites (a fenda das fitas agora e so decorativa)
        pocket() slot2d();
        pocket() dpad_pos();
        pocket() tri_pos();
        pocket() dot_pos();

        // botoes
        translate([0, 0, -1]) linear_extrude(front_t + 2) {
            for (c = pill_cs) pill_hole2d(c);
            round_hole2d(green_c, green_d);
            round_hole2d(red_c, red_d);
        }

        // furos guia dos pinos da placa de botoes
        for (p = btn_posts)
            translate([p[0], p[1], board_z - 5]) cylinder(d = post_pilot, h = 6);

        // laterais: letras, furinhos e o potenciometro no ombro esquerdo
        for (side = [0, 1]) {
            on_side(side, name_c) translate([0, 0, -name_pocket]) linear_extrude(name_pocket + 0.01) name2d();
            on_side(side, dots_c) translate([0, 0, -wall - 1]) linear_extrude(wall + 2) dots2d();
        }
        pot_cut();

        // face de baixo: entrada do cartao microSD e USB de carga
        bottom_cut(sd_x_card, sd_card_z) square(card_hole, center = true);
        translate([sd_x_card - card_recess[0] / 2, -0.01, sd_card_z - card_recess[1] / 2])
            cube([card_recess[0], card_recess[2], card_recess[1]]);
        bottom_cut(tp_port_x, tp_port_z) rrect_c(tp_usb_hole, 1.5);

        // parafusos da tampa
        for (y = screw_y) for (x = [-1, W - wall - 1])
            translate([x, y, screw_z]) rotate([0, 90, 0]) cylinder(d = screw_d_wall, h = wall + 2);
    }
}

// Abre um furo 2D (desenhado em x/z) atravessando a parede de baixo.
module bottom_cut(x, z) {
    translate([x, wall + 1, z]) rotate([90, 0, 0]) linear_extrude(wall + 2) children();
}

module pocket() {
    translate([0, 0, -0.01]) linear_extrude(inlay_pocket + 0.01) children();
}

module oled_ribs() {
    c = [scr_c[0], scr_c[1] - oled_aa_dy];
    s = oled_pcb + [0.4, 0.4];
    leg = 5;
    t = 1.2;
    translate([c[0], c[1], front_t]) linear_extrude(oled_rib_h)
        for (mx = [0, 1], my = [0, 1]) mirror([mx, 0]) mirror([0, my])
            translate([s[0] / 2, s[1] / 2]) {
                translate([0, -leg]) square([t, leg + t]);
                translate([-leg, 0]) square([leg + t, t]);
            }
}

// ESP32 de pe na parede direita: dois batentes na frente e um apoio embaixo.
module esp32_supports() {
    x0 = esp_pcb_x - 0.7;
    xw = W - wall - x0 + 0.01;
    for (y = [50, 80]) translate([x0, y, 0]) cube([xw, 4, esp_z0]);
    translate([x0, esp_y0 - 3, front_t]) cube([xw, 3, esp_w + esp_z0 - front_t]);
}

// Reforco por dentro da lateral esquerda, na espessura da rosca do pot.
module pot_boss() {
    translate([0, pot_c[0], pot_c[1]]) rotate([0, 90, 0]) cylinder(d = pot_body_d, h = pot_boss_t);
}

module pot_cut() {
    translate([-1, pot_c[0], pot_c[1]]) rotate([0, 90, 0]) {
        cylinder(d = pot_hole_d, h = pot_boss_t + 2);
        cylinder(d = nut_cb_d, h = nut_cb_h + 1);
    }
}

// =====================================================================
//  tampa (desenhada com a face externa em z=0 e o interior para +z)
// =====================================================================
// Converte "altura acima da face interna da tampa" para o z local da tampa.
function lid_z(h) = lid_t + h;

module open_frame(pos, size, h, open_bottom = true) {
    linear_extrude(h) difference() {
        translate(pos - [bay_wall, bay_wall]) square(size + [2 * bay_wall, 2 * bay_wall]);
        translate(pos) square(size);
        // lado de baixo aberto: por ali saem o cartao e o USB
        if (open_bottom) translate([pos[0], pos[1] - bay_wall - 1]) square([size[0], bay_wall + 2]);
    }
}

module lid() {
    difference() {
        union() {
            chamfered_body(lid_t, edge_chamfer, 0);
            translate([0, 0, lid_t]) {
                // aba interna (esquerda, direita e topo; a base fica livre)
                linear_extrude(lip_h) intersection() {
                    difference() {
                        inner_outline(lip_gap);
                        inner_outline(lip_gap + lip_t);
                    }
                    translate([0, 8]) square([W, H]);
                }
                // reforcos dos parafusos
                for (y = screw_y) for (x = [wall + lip_gap, W - wall - lip_gap - boss_w])
                    translate([x, y - boss_l / 2, 0]) cube([boss_w, boss_l, lip_h]);
                // degrau e berco da bateria
                translate(batt_pos - [bay_wall, bay_wall])
                    cube([batt[0] + 1 + 2 * bay_wall, batt[1] + 1 + 2 * bay_wall, batt_platform]);
                translate([0, 0, batt_platform])
                    open_frame(batt_pos, [batt[0] + 1, batt[1] + 1], bay_h, false);
                // moldura do microSD + apoios sob as bordas da placa
                open_frame(sd_pos, sd_pcb + [0.4, 0.4], sd_standoff + sd_pcb_t + 1);
                for (x = [sd_pos[0] + 0.5, sd_pos[0] + sd_pcb[0] - 2])
                    translate([x, sd_pos[1], 0]) cube([1.5, sd_pcb[1], sd_standoff]);
                // moldura do TP4056 + apoios
                open_frame(tp_pos, tp_size + [0.4, 0.4], tp_standoff + 1.6 + 0.5);
                for (x = [tp_pos[0] + 0.5, tp_pos[0] + tp_size[0] - 2])
                    translate([x, tp_pos[1] + 6, 0]) cube([1.5, tp_size[1] - 6, tp_standoff]);
            }
        }
        // furos guia dos parafusos (atravessam aba e reforco)
        for (y = screw_y)
            translate([-1, y, lid_z(lip_h / 2)]) rotate([0, 90, 0]) cylinder(d = screw_d_pilot, h = W + 2);
        // fenda da chave liga/desliga
        translate([sw_c[0], sw_c[1], -1]) linear_extrude(lid_t + 2) square(sw_slot, center = true);
        // clipe de metal: encaixe em V e furo guia cego do parafuso
        translate([clip_c[0], clip_c[1], -1]) linear_extrude(clip_recess + 1) clip_guide2d();
        translate([clip_c[0], clip_c[1] + clip_hole_y, -1])
            cylinder(d = clip_pilot_d, h = 1 + clip_recess + clip_pilot_depth);
    }
}

module lid_in_place() { translate([0, 0, D]) mirror([0, 0, 1]) lid(); }
// tampa virada para imprimir (rotacao, nao espelho, para manter a quiralidade)
module lid_print() { translate([W, 0, D]) rotate([0, 180, 0]) lid_in_place(); }

// =====================================================================
//  pecas coloridas
// =====================================================================
module inlay(t = inlay_t) { linear_extrude(t) offset(-tol) children(); }

// visor em relevo: entra 1 mm no rebaixo e sobressai frame_raise, com chanfro
module frame_part() {
    t = inlay_pocket + frame_raise;
    difference() {
        hull() {
            linear_extrude(t - frame_chamfer) offset(-tol) screen2d();
            linear_extrude(t) offset(-tol - frame_chamfer) screen2d();
        }
        translate([0, 0, -1]) linear_extrude(t + 2)
            translate(scr_c) square(oled_aa + [2, 2] * oled_win_margin, center = true);
    }
}
module letters_part() { linear_extrude(name_t) offset(-tol) name2d(); }
module slot_part() { inlay() slot2d(); }

// botao: aba na mesa, haste para cima
module cap(shape_size, is_round) {
    shank_h = front_t + cap_protrude;
    linear_extrude(flange_t)
        if (is_round) circle(d = shape_size + 2 * flange_extra + 2 * cap_clear);
        else stadium(shape_size + [2, 2] * (flange_extra + cap_clear));
    translate([0, 0, flange_t - 0.01]) linear_extrude(shank_h + 0.01)
        if (is_round) circle(d = shape_size);
        else stadium(shape_size);
}

// knob: topo na mesa, furo do eixo para cima
module knob_part() {
    difference() {
        union() {
            cylinder(d1 = knob_d - 1.6, d2 = knob_d, h = 0.8);
            translate([0, 0, 0.8]) cylinder(d = knob_d, h = knob_h - 0.8);
        }
        translate([0, 0, knob_h - shaft_hole_h]) cylinder(d = shaft_d, h = shaft_hole_h + 1);
    }
}

// Formato da base do clipe de metal: largo em cima, afinando em V para baixo.
module clip_guide2d() {
    offset(delta = clip_clear) polygon([
        [-clip_top_w / 2, clip_guide_h / 2], [clip_top_w / 2, clip_guide_h / 2],
        [clip_bottom_w / 2, -clip_guide_h / 2], [-clip_bottom_w / 2, -clip_guide_h / 2]]);
}

// =====================================================================
//  montagem com componentes fantasmas
// =====================================================================
module ghosts() {
    // OLED
    color("navy", 0.6) translate([scr_c[0] - oled_pcb[0] / 2, scr_c[1] - oled_aa_dy - oled_pcb[1] / 2, front_t + 1.5])
        cube([oled_pcb[0], oled_pcb[1], 1.2]);
    // placa de botoes e tact switches
    color("green", 0.5) translate([btn_board[0], btn_board[1], board_z]) cube([btn_board[2], btn_board[3], board_t]);
    for (c = concat(pill_cs, [red_c, green_c])) color("#333", 0.8)
        translate([c[0] - tact_size / 2, c[1] - tact_size / 2, board_z - tact_h]) cube([tact_size, tact_size, tact_h]);
    // ESP32 (placa + modulo + pinos soldados direto, sem dupont)
    color("black", 0.6) translate([esp_pcb_x, esp_y0, esp_z0]) cube([esp_pcb_t, esp_l, esp_w]);
    color("silver", 0.6) translate([esp_pcb_x + esp_pcb_t, esp_y0 + esp_l - 25, esp_z0 + (esp_w - 18) / 2]) cube([esp_mod_t, 25, 18]);
    color("gold", 0.4) translate([esp_pcb_x - 8.5, esp_y0, esp_z0]) cube([8.5, esp_l, esp_w]);
    // potenciometro
    color("gray", 0.7) translate([pot_boss_t, pot_c[0], pot_c[1]]) rotate([0, 90, 0]) cylinder(d = pot_body_d, h = pot_body_l);
    // bateria, microSD, TP4056, MT3608 e chave
    color("orange", 0.5) translate([batt_pos[0] + 0.5, batt_pos[1] + 0.5, inner_back - batt_platform - batt[2]]) cube(batt);
    color("blue", 0.6) translate([sd_pos[0], sd_socket_y, inner_back - sd_standoff - sd_pcb_t - 3.5])
        cube([sd_pcb[0], sd_pcb[1] + sd_socket_out, sd_pcb_t + 3.5]);
    color("black", 0.8) translate([sd_x_card - 5.5, 0, sd_card_z - 0.4]) cube([11, 15, 0.8]);   // cartao
    color("red", 0.5) translate([tp_pos[0], tp_pos[1], inner_back - tp_standoff - 4.6]) cube([tp_size[0], tp_size[1], 4.6]);
    color("purple", 0.5) translate([mt_box[0], mt_box[1], mt_box[2]]) cube([mt_box[3], mt_box[4], mt_box[5]]);
    color("white", 0.7) translate([sw_c[0] - 1.8, sw_c[1] - 4.3, inner_back - 3.7]) cube([3.6, 8.6, 3.7]);
}

module assembly() {
    color("#5fc4b0") front_shell();
    color("#55b3a1") lid_in_place();
    color("#c8f0d8") translate([0, 0, inlay_pocket]) mirror([0, 0, 1]) frame_part();
    for (side = [0, 1]) color("#111")
        on_side(side, name_c) translate([0, 0, -name_pocket]) letters_part();
    color("#222") translate([0, 0, -inlay_t + inlay_pocket]) slot_part();
    color("gold")      translate([0, 0, -inlay_t + inlay_pocket]) inlay() dpad_pos();
    color("skyblue")   translate([0, 0, -inlay_t + inlay_pocket]) inlay() tri_pos();
    color("royalblue") translate([0, 0, -inlay_t + inlay_pocket]) inlay() dot_pos();
    for (c = pill_cs) color("royalblue")
        translate([c[0], c[1], front_t + flange_t]) mirror([0, 0, 1]) cap(pill_size, false);
    color("limegreen") translate([green_c[0], green_c[1], front_t + flange_t]) mirror([0, 0, 1]) cap(green_d, true);
    color("crimson")   translate([red_c[0], red_c[1], front_t + flange_t]) mirror([0, 0, 1]) cap(red_d, true);
    // knob no ombro esquerdo, por fora da parede
    color("#222") translate([-0.5, pot_c[0], pot_c[1]]) rotate([0, 90, 0]) translate([0, 0, -knob_h]) knob_part();
    ghosts();
}

// =====================================================================
//  Tudo acima e desenhado como se x crescesse para a DIREITA de quem olha a
//  frente do BMO. Como a frente fica em z=0 (virada para -z), espelhamos em x
//  no final para que cada lado fique onde deve.
// =====================================================================
module oriented() { translate([W, 0, 0]) mirror([1, 0, 0]) children(); }

oriented() {
    if (part == "montagem")       assembly();
    else if (part == "frente")    front_shell();
    else if (part == "tampa")     lid_print();
    else if (part == "moldura")   frame_part();
    else if (part == "letras")    oriented() letters_part();
    else if (part == "fenda")     slot_part();
    else if (part == "vermelho")  cap(red_d, true);
    else if (part == "verde")     cap(green_d, true);
    else if (part == "pilula")    cap(pill_size, false);
    else if (part == "knob")      knob_part();
    else if (part == "cruz")      inlay() dpad_pos();
    else if (part == "triangulo") inlay() tri_pos();
    else if (part == "bolinha")   inlay() dot_pos();
}
