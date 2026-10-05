// =====================================================================
//  Base de mesa para o BMO do Localfy
// =====================================================================
//  O BMO encaixa num rebaixo no topo da base. Um poco sob o USB de carga e
//  um canal por baixo deixam o cabo ligado com o BMO na base: o cabo sai
//  por tras.
//
//  Le as medidas da case (bmo_case.scad), entao acompanha mudancas nela.
//
//  peca = "base"    - para imprimir (ja na posicao: fundo na mesa, sem suporte)
//  peca = "previa"  - base com o BMO encaixado, so para visualizar
//
//  openscad -D 'peca="base"' -o stl/base.stl bmo_base.scad
// =====================================================================

include <bmo_case.scad>
part = "nenhuma";   // desliga a montagem da case (a ultima atribuicao vence)

peca = "previa";

// ---------------- base ----------------
base_margin = 12;      // borda da base em volta do BMO
base_r = 10;           // raio dos cantos
base_h = 24;           // altura total
base_chamfer = 1.5;    // chanfro da borda de cima
foot_chamfer = 0.6;    // chanfro de baixo (evita a "pata de elefante" da 1a camada)
pocket_d = 6;          // profundidade do encaixe (abaixo do knob de volume)
pocket_clear = 0.4;    // folga por lado

// Cabo USB: poco vertical sob o conector e canal ate a parte de tras.
usb_well = [14, 10];   // abertura para o corpo do plugue (x, profundidade)
channel_w = 12;
channel_h = 10;

// Nome gravado na frente da base.
label = "LOCALFY";
label_size = 6;
label_depth = 0.6;

// ---------------- referencias tiradas da case ----------------
// Na base, o eixo X e a largura do BMO, Y e a profundidade (a frente do
// BMO fica em Y=0 e as costas em Y=-D) e Z e a altura.
floor_z = base_h - pocket_d;                 // onde o fundo do BMO apoia
usb_xy = [W - tp_port_x, -tp_port_z];        // USB de carga na face de baixo
base_min = [-base_margin, -D - base_margin];
base_size = [W + 2 * base_margin, D + 2 * base_margin];

module base_outline(inset = 0) {
    translate(base_min) offset(-inset) rrect(base_size, base_r);
}

module base_body() {
    hull() {
        linear_extrude(0.01) base_outline(foot_chamfer);
        translate([0, 0, foot_chamfer]) linear_extrude(base_h - foot_chamfer - base_chamfer) base_outline();
        translate([0, 0, base_h - 0.01]) linear_extrude(0.01) base_outline(base_chamfer);
    }
}

module stand() {
    difference() {
        base_body();

        // encaixe do BMO
        translate([-pocket_clear, -D - pocket_clear, floor_z])
            cube([W + 2 * pocket_clear, D + 2 * pocket_clear, pocket_d + 1]);

        // poco do plugue USB, do encaixe ate a mesa
        translate([usb_xy[0], usb_xy[1], -1]) linear_extrude(floor_z + 2) rrect_c(usb_well, 2);

        // canal do cabo por baixo, saindo pela parte de tras
        translate([usb_xy[0] - channel_w / 2, base_min[1] - 1, -0.01])
            cube([channel_w, usb_xy[1] - base_min[1] + 1, channel_h]);

        // nome gravado na frente
        // (girado para ler da frente; o texto cresce para fora a partir de
        //  label_depth para dentro da face)
        translate([W / 2, base_min[1] + base_size[1] - label_depth, floor_z / 2])
            rotate([90, 0, 180]) linear_extrude(label_depth + 0.01)
                text(label, size = label_size, font = name_font, halign = "center",
                     valign = "center", spacing = 1.2);
    }
}

// O BMO montado, levado do sistema da case (y para cima, z para tras) para
// o da base (z para cima, y para frente). E uma rotacao, nao um espelho.
module bmo_on_stand() {
    multmatrix([[1, 0, 0, 0], [0, 0, -1, 0], [0, 1, 0, floor_z], [0, 0, 0, 1]])
        oriented() assembly();
}

if (peca == "base") stand();
else if (peca == "previa") {
    color("#f2f2f2") stand();
    bmo_on_stand();
}
