// =====================================================================
//  Figuras do BMO para o artigo (gera PNGs com cotas e rotulos).
//  Uso: ./gerar_figuras.sh   (todas)   |   ./gerar_figuras.sh frente
//
//  Tudo e montado num referencial "em pe": X = largura, Y = profundidade
//  (frente em Y=0, costas em Y=-40), Z = altura. Os rotulos usam os mesmos
//  angulos da camera (variavel cam) para ficarem de frente para ela.
//  Atencao: na vista de frente (camera de +Y), +X aparece a ESQUERDA.
// =====================================================================
include <bmo_case.scad>
include <stl_limites.scad>
part = "nenhuma";   // desliga a montagem da case (a ultima atribuicao vence)

fig = "geral";
cam = [70, 0, 210];

$fn = 48;
ink = "#263238";
text_size = 3.2;

// ---------------- referencial em pe ----------------
module standing() { rotate([90, 0, 0]) children(); }   // (x, y, z) -> (x, -z, y)
function P(p) = [W - p[0], -p[2], p[1]];   // ponto do modelo -> em pe

// ---------------- rotulos e cotas ----------------
module billboard_text(t, size = text_size, h = "center", v = "center") {
    rotate(cam) linear_extrude(0.3) text(t, size = size, font = "Liberation Sans:style=Bold", halign = h, valign = v);
}
module line3d(a, b, d = 0.45) { hull() { translate(a) sphere(d = d, $fn = 8); translate(b) sphere(d = d, $fn = 8); } }

// Direcao "para a direita da tela" na camera atual: rotate(cam) aplicado a [1,0,0]
// (rotate([a,b,c]) = Rz(c) * Ry(b) * Rx(a)).
function screen_right() =
    let(b = cam[1], c = cam[2]) [cos(c) * cos(b), sin(c) * cos(b), -sin(b)];

// Rotulo: ponto 'a' na peca, linha ate 'p', texto comecando em p + gap.
// side = +1: texto cresce para a direita da tela; -1: para a esquerda.
module label(a, p, t, side = 1) {
    color(ink) {
        line3d(a, p);
        translate(a) sphere(d = 1.4, $fn = 12);
        translate(p + screen_right() * side * 1.5) billboard_text(t, h = side > 0 ? "left" : "right");
    }
}

// Cota entre a e b, afastada por 'off'; texto no meio (+ toff).
module dim(a, b, off, t, toff = [0, 0, 0]) {
    a2 = a + off; b2 = b + off;
    u = off / norm(off);
    color(ink) {
        line3d(a, a2 + u * 1.5, 0.3);
        line3d(b, b2 + u * 1.5, 0.3);
        line3d(a2, b2, 0.4);
        for (e = [[a2, b2], [b2, a2]]) {
            dir = (e[1] - e[0]) / norm(e[1] - e[0]);
            line3d(e[0], e[0] + dir * 2.5 + cross(dir, u) * 0.9, 0.4);
            line3d(e[0], e[0] + dir * 2.5 - cross(dir, u) * 0.9, 0.4);
        }
        translate((a2 + b2) / 2 + u * 3.5 + toff) billboard_text(t);
    }
}

// ---------------- pecas do BMO ----------------
module oled_glass() {   // vidro escuro do OLED atras da janela
    color("#0d1b2a") translate([scr_c[0] - oled_pcb[0] / 2, scr_c[1] - oled_aa_dy - oled_pcb[1] / 2, front_t + 0.1]) cube([oled_pcb[0], oled_pcb[1], 1.4]);
}
module bmo_front_group() {
    color("#5fc4b0") front_shell();
    oled_glass();
    color("#c8f0d8") translate([0, 0, inlay_pocket]) mirror([0, 0, 1]) frame_part();
    for (side = [0, 1]) color("#111") on_side(side, name_c) translate([0, 0, -name_pocket]) letters_part();
    color("#222") translate([0, 0, -inlay_t + inlay_pocket]) slot_part();
    color("gold")      translate([0, 0, -inlay_t + inlay_pocket]) inlay() dpad_pos();
    color("skyblue")   translate([0, 0, -inlay_t + inlay_pocket]) inlay() tri_pos();
    color("royalblue") translate([0, 0, -inlay_t + inlay_pocket]) inlay() dot_pos();
    for (c = pill_cs) color("royalblue") translate([c[0], c[1], front_t + flange_t]) mirror([0, 0, 1]) cap(pill_size, false);
    color("limegreen") translate([green_c[0], green_c[1], front_t + flange_t]) mirror([0, 0, 1]) cap(green_d, true);
    color("crimson")   translate([red_c[0], red_c[1], front_t + flange_t]) mirror([0, 0, 1]) cap(red_d, true);
    color("#222") translate([-0.5, pot_c[0], pot_c[1]]) rotate([0, 90, 0]) translate([0, 0, -knob_h]) knob_part();
}

// Componentes presos na frente (cada um com sua cor; 'cut' aplica o corte).
module comp_front(i) {
    if (i == 0) color("#1b263b") translate([scr_c[0] - oled_pcb[0] / 2, scr_c[1] - oled_aa_dy - oled_pcb[1] / 2, front_t + 1.5]) cube([oled_pcb[0], oled_pcb[1], 1.2]);
    if (i == 1) color("#2e7d32") translate([btn_board[0], btn_board[1], board_z]) cube([btn_board[2], btn_board[3], board_t]);
    if (i == 2) color("#37474f") for (c = concat(pill_cs, [red_c, green_c])) translate([c[0] - tact_size / 2, c[1] - tact_size / 2, board_z - tact_h]) cube([tact_size, tact_size, tact_h]);
    if (i == 3) color("#212121") translate([esp_pcb_x, esp_y0, esp_z0]) cube([esp_pcb_t, esp_l, esp_w]);
    if (i == 4) color("#b0bec5") translate([esp_pcb_x + esp_pcb_t, esp_y0 + esp_l - 25, esp_z0 + (esp_w - 18) / 2]) cube([esp_mod_t, 25, 18]);
    if (i == 5) color("#9e9e9e") translate([pot_boss_t, pot_c[0], pot_c[1]]) rotate([0, 90, 0]) cylinder(d = pot_body_d, h = pot_body_l);
    if (i == 6) color("#7b1fa2") translate([mt_box[0], mt_box[1], mt_box[2]]) cube([mt_box[3], mt_box[4], mt_box[5]]);
}
// Componentes presos na tampa (posicao com a tampa fechada).
module comp_lid(i) {
    if (i == 0) color("#ef6c00") translate([batt_pos[0] + 0.5, batt_pos[1] + 0.5, inner_back - batt_platform - batt[2]]) cube(batt);
    if (i == 1) color("#1565c0") translate([sd_pos[0], sd_socket_y, inner_back - sd_standoff - sd_pcb_t - 3.5]) cube([sd_pcb[0], sd_pcb[1] + sd_socket_out, sd_pcb_t + 3.5]);
    if (i == 2) color("#c62828") translate([tp_pos[0], tp_pos[1], inner_back - tp_standoff - 1.6 - tp_usbc_h]) cube([tp_size[0], tp_size[1], 1.6 + tp_usbc_h]);
    if (i == 3) color("#eceff1") translate([sw_c[0] - sw_body[0] / 2, sw_c[1] - sw_body[1] / 2, inner_back - sw_body[2]]) cube(sw_body);
}
module lid_group() { color("#4aa595") lid_in_place(); for (i = [0 : 3]) comp_lid(i); }

module bmo_closed() { standing() oriented() { bmo_front_group(); color("#4aa595") lid_in_place(); } }

// Corte no meio da largura: a face cortada fica com a cor 'c'.
module cut(c) { intersection() { color(c) children(); color(c) translate([-1, -100, -50]) cube([W / 2 + 1, 200, 200]); } }

// ---------------- figuras ----------------
if (fig == "geral") bmo_closed();

if (fig == "frente") {   // camera de +Y: +X aparece a esquerda da tela
    bmo_closed();
    y0 = 3;
    dim([W, y0, 0], [0, y0, 0], [0, 0, -9], "60 mm");
    dim([W, y0, 0], [W, y0, H], [10, 0, 0], "100 mm", [6, 0, 0]);
    dim([W - (scr_c[0] - scr_size[0] / 2), y0, H - 2], [W - (scr_c[0] + scr_size[0] / 2), y0, H - 2], [0, 0, 8], "visor 44 x 38 mm");
    // lado direito da tela (X negativo)
    label(P([scr_c[0] + 6, scr_c[1], -4]), [-14, y0, 73], "tela OLED 0,96\"", 1);
    label(P([green_c[0], green_c[1], -4]), [-14, y0, 30], "play / pause", 1);
    label(P([red_c[0], red_c[1], -4]), [-14, y0, 12], "trocar de modo", 1);
    // lado esquerdo da tela (X alem de W)
    label(P([pill_cs[1][0], pill_cs[1][1], -4]), [W + 22, y0, 30], "proxima", -1);
    label(P([pill_cs[0][0], pill_cs[0][1], -4]), [W + 22, y0, 20], "anterior", -1);
}

if (fig == "lateral") {  // lateral esquerda do BMO (lado do volume)
    bmo_closed();
    x0 = W + 3;
    dim([x0, 0, 0], [x0, -D, 0], [0, 0, -9], "40 mm");
    dim([x0, -D, 0], [x0, -D, H], [0, -9, 0], "100 mm", [0, -6, 0]);
    label(P([-4, dots_c[0], dots_c[1]]), [x0, 16, 92], "furos (padrao do BMO)", 1);
    label(P([-4, name_c[0], name_c[1]]), [x0, 16, 62], "nome BMO + versao ev1", 1);
    label(P([-6, pot_c[0], pot_c[1]]), [x0, 16, 28], "volume (knob no ombro)", 1);
    label(P([-1, screw_y[1], screw_z]), [x0, -D - 20, 76], "parafusos da tampa", -1);
}

if (fig == "explodida") {  // por tras: tampa afastada, interior a mostra
    gap = 38;
    standing() oriented() {
        bmo_front_group();
        for (i = [0 : 6]) comp_front(i);
        translate([0, 0, gap]) lid_group();
        for (y = screw_y) for (s = [0, 1]) color("orange")
            translate([s == 0 ? -13 : W + 5, y, screw_z + gap / 2]) rotate([0, 90, 0]) cylinder(d = 2, h = 8);
    }
    label(P([W / 2, H, D + gap]), P([W / 2 + 25, H + 14, D + gap]), "tampa", 1);
    label(P([W / 2, H, D / 2]), P([W / 2 + 25, H + 14, D / 2]), "frente + paredes", 1);
    label(P([-9, screw_y[1], screw_z + gap / 2]), P([-30, screw_y[1] + 22, screw_z + gap / 2]), "4 parafusos M2", 1);
}

if (fig == "interior") {   // sem a tampa, vista por tras
    standing() oriented() { bmo_front_group(); for (i = [0 : 6]) comp_front(i); }
    lx = -16; rx = W + 16; yb = -D - 6;
    label(P([esp_pcb_x, esp_y0 + 40, esp_z0 + esp_w / 2]), [lx, yb, 88], "ESP32 (antena no alto)", -1);
    label(P([mt_box[0] + 30, mt_box[1] + 8, mt_box[2] + mt_box[5]]), [lx, yb, 10], "regulador MT3608", -1);
    label(P([scr_c[0], scr_c[1] - oled_aa_dy, front_t + 2.7]), [rx, yb, 86], "OLED", 1);
    label(P([btn_board[0] + 30, btn_board[1] + 22, board_z + board_t]), [rx, yb, 42], "placa dos 4 botoes", 1);
    label(P([pot_boss_t + pot_body_l, pot_c[0], pot_c[1]]), [rx, yb, 22], "potenciometro (volume)", 1);
}

if (fig == "tampa") {    // tampa por dentro, com os modulos
    standing() oriented() lid_group();
    lx = W + 16; rx = -16; yf = 2;
    label(P([batt_pos[0] + 17, batt_pos[1] + 25, inner_back - batt_platform - batt[2]]), [lx, yf, 72], "bateria LiPo 1800 mAh", -1);
    label(P([sd_x_card, 14, inner_back - 5]), [lx, yf, 14], "modulo microSD", -1);
    label(P([tp_port_x, 14, inner_back - 5]), [rx, yf, 10], "carregador USB-C", 1);
    label(P([sw_c[0], sw_c[1], inner_back - sw_body[2]]), [rx, yf, 36], "chave liga/desliga", 1);
}

if (fig == "corte") {    // corte no meio da largura, visto pela lateral
    standing() oriented() {
        cut("#5fc4b0") front_shell();
        cut("#4aa595") lid_in_place();
        cut("#c8f0d8") translate([0, 0, inlay_pocket]) mirror([0, 0, 1]) frame_part();
        for (i = [0 : 6]) cut(["#1b263b", "#2e7d32", "#37474f", "#212121", "#b0bec5", "#9e9e9e", "#7b1fa2"][i]) comp_front(i);
        for (i = [0 : 3]) cut(["#ef6c00", "#1565c0", "#c62828", "#eceff1"][i]) comp_lid(i);
    }
    // camera pelo lado do corte (-X): a frente aparece a esquerda da tela
    xs = W / 2 - 2;
    label(P([W / 2, scr_c[1], front_t + 2]), [xs, 16, 74], "OLED atras do visor", -1);
    label(P([W / 2, 13, board_z + 1]), [xs, 16, 12], "placa dos botoes", -1);
    label(P([W / 2, batt_pos[1] + 30, inner_back - batt_platform - 5]), [xs, -D - 16, 72], "bateria (degrau na tampa)", 1);
    label(P([W / 2, 14, inner_back - 4]), [xs, -D - 16, 18], "carregador e microSD", 1);
    dim([xs, 0, H + 2], [xs, -D, H + 2], [0, 0, 8], "40 mm");
}

if (fig == "base") {
    translate([0, 0, 18]) bmo_closed();
    color("#eceff1") import("stl/base.stl");
    dim([-12, 14, 0], [W + 12, 14, 0], [0, 6, -4], "84 mm");
}

if (fig == "pecas") {    // pecas para imprimir, dispostas em grade
    layout = [   // nome, posicao (x, y), cor
        ["frente", [0, 0], "#5fc4b0"], ["tampa", [72, 0], "#5fc4b0"], ["base", [146, 18], "#eceff1"],
        ["moldura", [0, -58], "#c8f0d8"], ["letras", [56, -48], "#222222"], ["fenda", [116, -45], "#222222"],
        ["vermelho", [150, -48], "crimson"], ["verde", [170, -48], "limegreen"], ["pilula", [188, -45], "royalblue"],
        ["knob", [206, -48], "#222222"], ["cruz", [228, -50], "gold"], ["triangulo", [252, -48], "skyblue"],
        ["bolinha", [276, -45], "royalblue"]];
    for (it = layout) {
        lim = [for (s = stl_limits) if (s[0] == it[0]) s][0];
        size = lim[2] - lim[1];
        translate([it[1][0], it[1][1], 0]) {
            if (it[0] == "frente")   // mostra a frente para cima (rotacao, nao espelho)
                color(it[2]) translate([size[0], 0, size[2]]) rotate([0, 180, 0]) translate(-lim[1]) import("stl/frente.stl");
            else
                color(it[2]) translate(-lim[1]) import(str("stl/", it[0], ".stl"));
            color(ink) translate([size[0] / 2, -6, 0]) billboard_text(it[0], size = 3.6);
        }
    }
}
