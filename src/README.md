# Firmware do Localfy: como o código está organizado

O firmware é dividido em **módulos**, cada um numa pasta e com uma única
responsabilidade. Cada módulo tem um `.h`, que diz *o que* ele oferece (comece
a leitura por aqui), e um `.cpp`, que diz *como* faz.

```
src/
├── main.cpp            liga tudo na ordem certa (setup) e roda o laço (loop)
├── config/
│   ├── Config.h        TODOS os pinos, tempos, limites e caminhos
│   └── secrets.h       Wi-Fi e nome do fone (fora do Git; copie o .example)
├── core/               infraestrutura usada por todos
│   ├── Lock            trava entre tarefas (Guard)
│   ├── BootMode        modo Bluetooth / Web e troca com reinício
│   ├── Diagnostics     mensagens [HEAP] [LOOP] [AUDIO] na serial
│   └── TimeUtil.h      "o relógio já foi acertado?"
├── storage/            tudo que mora no cartão SD
│   ├── Storage         monta o cartão
│   ├── Library         lista de músicas (/music)
│   ├── MetaStore       artista e gênero (meta.json)
│   ├── History         histórico de reprodução (history.csv)
│   └── Settings        fone escolhido (memória interna, NVS)
├── audio/
│   ├── Player          o que toca, play/pause, próxima, volume, histórico
│   ├── AudioPipeline   arquivo → decoder MP3 → buffer → Bluetooth
│   └── Duration        duração de MP3/WAV
├── bluetooth/
│   └── Bluetooth       conexão A2DP com o fone
├── net/
│   └── WifiLink        rede própria do Localfy (ponto de acesso), só no modo Web
├── web/                servidor do modo Web
│   ├── WebServer       sobe o servidor e registra as rotas
│   ├── Http            respostas JSON e leitura de corpo JSON
│   └── Routes*.cpp     rotas por assunto: Ui, Library, Player, Stats, Bluetooth
├── ui/                 hardware de interface
│   ├── Button          botão com debounce (clique / pressão longa)
│   ├── VolumeKnob      potenciômetro de volume
│   ├── StatusLed       LED do modo Web
│   ├── Display         desenha as telas no OLED
│   └── UiState.h       qual tela está aberta
└── app/
    └── App             "cérebro": botões e knob → ações e telas
```

## Regras da casa

- **Dependências vão para baixo.** `app` usa `ui`, `audio`, `storage`... mas
  `storage` não sabe que existe tela, nem `audio` sabe que existe botão.
- **Nada de número solto.** Pino, tempo ou limite novo vai para
  `config/Config.h`, com um comentário dizendo o porquê.
- **Estado é privado.** Cada `.cpp` guarda o próprio estado num
  `namespace { ... }` e só expõe funções. Ninguém mexe em variável de outro módulo.
- **Use a trava.** O servidor web e o decoder rodam em outras tarefas. Quem
  muda o player ou mexe no cartão faz isso dentro de `{ Guard g; ... }`.
  Nunca crie um `Guard` dentro de outro (a trava não é recursiva).

## Onde mexer para...

| Quero... | Arquivo |
|---|---|
| trocar um pino ou um tempo | `config/Config.h` |
| trocar o fone ou a rede Wi-Fi | `config/secrets.h` |
| mudar o que um botão faz | `app/App.cpp` |
| mudar o desenho de uma tela | `ui/Display.cpp` |
| criar uma rota na API | o `web/Routes*.cpp` do assunto |
| mudar regra do histórico (tempo mínimo etc.) | `config/Config.h` (`scrobble`) e `audio/Player.cpp` |

## Modos de operação

O ESP32 sem PSRAM não tem RAM para Wi-Fi e Bluetooth juntos. Por isso:

- **Bluetooth** (padrão ao ligar): toca no fone, controlado pelos botões.
- **Web**: Wi-Fi + interface web para gerenciar o acervo; LED branco aceso.

Para trocar: aperte o **vermelho** (abre a pergunta) e confirme no **verde**.
Se apertar o vermelho sem querer, aperte de novo para cancelar (ou espere 10 s).
A música para, a tela avisa o novo modo e o aparelho reinicia já nele.

**Navegar nas músicas:** no player, um **azul** abre a lista na pasta que está
tocando (o primeiro toque só abre). Azuis movem a seleção; **segurar um azul** pula
uma página inteira (e continua pulando enquanto segura). **Verde** abre a pasta ou
toca a música, e a pasta vira a fila. **Vermelho** sobe um nível: músicas → pastas
→ player. A lista fecha sozinha após 8 s parada.

**Aleatório:** na lista de pastas, o primeiro item é **"Aleatorio SIM/NAO"**; o
**verde** liga e desliga. Escolha "Todas" (biblioteca inteira) ou uma pasta (só
aquele artista). Com ele ligado, a barra do player mostra **"ALE"**. Fica salvo.

**Trocar de fone (modo Bluetooth):** segure o **vermelho** por 3 s. A música para,
o fone atual desconecta e aparece a lista de aparelhos de áudio por perto (deixe o
novo fone em modo de pareamento). Azuis escolhem, **verde** conecta, **vermelho**
cancela e volta a procurar o fone anterior. A escolha fica salva na memória do
ESP32 e vale nos próximos boots. Se o fone desligar, o Localfy volta a procurá-lo
e reconecta sozinho.

**Modo Web:** o Localfy não entra em roteador nenhum. Ele cria a própria rede
**Localfy** (senha `Localfy271003!`, em `config/Config.h`) e mostra nome, senha e
endereço na tela. Entre nela pelo celular ou computador e abra `http://192.168.4.1`
(no celular, a interface costuma abrir sozinha). Como a rede não tem internet, a
interface acerta o relógio do aparelho com a hora do celular ao abrir.

**Standby:** segure o **verde** por 3 s. Aparece a cara
do BMO em cores invertidas (fundo claro) (pisca e troca de expressão); a música continua como estava. Para sair,
clique no **verde** (esse clique só sai, não pausa). Os outros botões ficam
inativos no standby; o knob continua mudando o volume.

## Compilar e gravar

```
pio run -t upload                  # firmware normal
pio run -e esp32dev-web -t upload  # sempre liga no modo Web (para mexer na interface)
pio device monitor                 # mensagens de diagnóstico
```

## Notas de arquitetura

1. **Por que não tem banco de dados.** O histórico é um CSV no próprio
   cartão: todo play vira registro, mesmo sem PC ligado. Para análise pesada,
   baixe `/api/history.csv`.
2. **SPI do SD × áudio.** Upload e reprodução dividem o barramento do cartão;
   em cartões lentos o áudio pode engasgar durante um upload grande.
3. **Duração de MP3.** A estimativa inicial erra em arquivos VBR; o player a
   refina enquanto toca, e a interface trata duração 0 como "desconhecida".
4. **Sem PSRAM.** A lista de músicas fica na RAM interna, limitada a 256
   faixas. Uma placa ESP32-WROVER (com PSRAM) resolveria isso, e também o
   limite de um modo por vez.
