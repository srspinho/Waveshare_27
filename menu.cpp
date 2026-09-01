#include "menu.h"
#include <EEPROM.h>
#include <hardware/watchdog.h>  // <--- ADICIONE ESTA LINHA NO TOPO
#include "Orbitron_Bold6pt7b.h"
#include "JetBrainsMono_SemiBold20pt7b.h"
#include "JetBrainsMono_Bold6pt7b.h"

// --- DECLARAÇÕES EXTERNAS (Vêm do arquivo principal .ino) ---
extern TFT_eSPI tft;        // Avisa que o objeto tft existe no .ino
extern TFT_eSprite canvas;  // Avisa que o canvas global existe no .ino

// Variáveis do Odômetro
extern uint32_t odom_total_vida;   // Resolve o erro de 'odom_total_vida'
extern uint32_t odom_teclas_hoje;  // Resolve o erro de 'odom_teclas_hoje'

// Fontes customizadas usadas na renderização
//extern const GFXfont Orbitron_Bold6pt7b;
//extern const GFXfont JetBrainsMono_Bold6pt7b;
//extern const GFXfont JetBrainsMono_SemiBold20pt7b;

// Protótipo da função de desenho do tambor
extern void desenhar_odometro(uint32_t valor, int x0, int cy, int boxW, int boxH, bool darPush);

// Inicializa as variáveis globais do menu
Config config;
MenuLevel current_menu_level = LEVEL_MAIN;
int current_item_index = 0;
bool is_editing_value = false;

unsigned long last_debounce_time = 0;
//const unsigned long DEBOUNCE_DELAY = 150;

// Estrutura para tratar o debounce individual de cada pino do joystick
struct ButtonDebounce {
  uint8_t pin;
  bool lastState;
  bool stableState;
  unsigned long lastDebounceTime;
};

// Instâncias para os 5 pinos do Joystick (iniciam em HIGH pois usam INPUT_PULLUP)
static ButtonDebounce btnUp = { JOY_UP, HIGH, HIGH, 0 };
static ButtonDebounce btnDown = { JOY_DOWN, HIGH, HIGH, 0 };
static ButtonDebounce btnLeft = { JOY_LEFT, HIGH, HIGH, 0 };
static ButtonDebounce btnRight = { JOY_RIGHT, HIGH, HIGH, 0 };
static ButtonDebounce btnOk = { JOY_OK, HIGH, HIGH, 0 };

const unsigned long DEBOUNCE_DELAY = 40;  // 40ms é o tempo ideal para chaves mecânicas/joysticks

// ==================== ITENS DOS MENUS ====================
#define MAIN_MENU_COUNT 6
const char* main_menu_items[MAIN_MENU_COUNT] = {
  "Screensaver", "Configuracoes", "Produtividade", "Sistema", "Odometro", "Sair"
};

#define SS_MENU_COUNT 11
const char* ss_menu_items[SS_MENU_COUNT] = {
  "Atari", "Pong", "Asteroids", "Star Field", "Game of Life",
  "Matrix", "Pac Man", "Fogos", "Aquario", "Todos", "Voltar"
};

#define CONFIG_MENU_COUNT 4
const char* config_menu_items[CONFIG_MENU_COUNT] = {
  "Veloc. Gradiente", "Qtd Asteroides", "Intensidade Cores", "Voltar"
};

#define PROD_MENU_COUNT 4
const char* prod_menu_items[PROD_MENU_COUNT] = {
  "Envio CTRL+Shift", "Intervalo", "Inicio", "Voltar"
};

#define SISTEMA_MENU_COUNT 3
const char* sistema_menu_items[SISTEMA_MENU_COUNT] = {
  "Brilho", "Reset", "Voltar"
};

// Protótipos internos locais
void processar_acao(char botao);
void executar_selecao();
void ajustar_valor_variavel(int direcao);
void executar_reset_placa();

void setup_menu() {
  // Configura todos os pinos do joystick como INPUT_PULLUP
  pinMode(JOY_UP, INPUT_PULLUP);
  pinMode(JOY_DOWN, INPUT_PULLUP);
  pinMode(JOY_LEFT, INPUT_PULLUP);
  pinMode(JOY_RIGHT, INPUT_PULLUP);
  pinMode(JOY_OK, INPUT_PULLUP);

  carregar_configuracoes();
}

#include "menu.h"

void desenhar_grafico_historico_ajustado(TFT_eSprite *canvas, const uint32_t historico[7], int16_t x_base, int16_t y_base) {
    const int16_t largura_barra = 12;
    const int16_t espacamento = 6;
    const int16_t altura_maxima = 75;

    // Encontra o maior valor para escalar as barras
    uint32_t valor_maximo = 1;
    for (int i = 0; i < 7; i++) {
        if (historico[i] > valor_maximo) {
            valor_maximo = historico[i];
        }
    }

    // Desenha as 7 barras do histórico
    for (int i = 0; i < 7; i++) {
        int16_t altura_barra = (int16_t)((historico[i] * altura_maxima) / valor_maximo);
        if (historico[i] > 0 && altura_barra < 2) altura_barra = 2; // Garante visibilidade mínima

        int16_t posX = x_base + i * (largura_barra + espacamento);
        int16_t posY = y_base - altura_barra;

        // Limpa a área da coluna
        canvas->fillRect(posX, y_base - altura_maxima, largura_barra, altura_maxima, TFT_BLACK);
        
        // Desenha a barra e borda
        if (altura_barra > 0) {
            canvas->fillRect(posX, posY, largura_barra, altura_barra, TFT_CYAN);
            canvas->drawRect(posX, posY, largura_barra, altura_barra, TFT_WHITE);
        }

        // Rótulo dos dias (1d a 7d)
        canvas->setTextDatum(TC_DATUM);
        canvas->setTextColor(TFT_SILVER, TFT_BLACK);
        canvas->drawString(String(i + 1) + "d", posX + (largura_barra / 2), y_base + 3, 1);
    }
}

// Desenha o gráfico de barras dos últimos 7 dias dentro da área do canvas
void desenhar_grafico_historico(TFT_eSprite *canvas, const uint32_t historico[7]) {
    const int16_t x_base = 15;
    const int16_t y_base = 140; // Linha de base do gráfico
    const int16_t largura_barra = 14;
    const int16_t espacamento = 6;
    const int16_t altura_maxima = 50;

    // Encontra o maior valor para escalar as barras proporcionalmente
    uint32_t valor_maximo = 1;
    for (int i = 0; i < 7; i++) {
        if (historico[i] > valor_maximo) {
            valor_maximo = historico[i];
        }
    }

    // Desenha as 7 barras
    for (int i = 0; i < 7; i++) {
        int16_t altura_barra = (int16_t)((historico[i] * altura_maxima) / valor_maximo);
        if (historico[i] > 0 && altura_barra == 0) altura_barra = 2; // Garante 2px mínimos de altura visual

        int16_t posX = x_base + i * (largura_barra + espacamento);
        int16_t posY = y_base - altura_barra;

        // Limpa fundo da barra
        canvas->fillRect(posX, y_base - altura_maxima, largura_barra, altura_maxima, TFT_BLACK);
        
        // Desenha a coluna de dados e borda
        canvas->fillRect(posX, posY, largura_barra, altura_barra, TFT_CYAN);
        canvas->drawRect(posX, posY, largura_barra, altura_barra, TFT_WHITE);

        // Exibe o dia da semana abreviado no topo/rodapé
        canvas->setTextDatum(TC_DATUM);
        canvas->setTextColor(TFT_SILVER, TFT_BLACK);
        canvas->drawString(String(i + 1) + "d", posX + (largura_barra / 2), y_base + 4, 1);
    }
}

void atualizarBrilho() {
#ifdef TFT_BL
  int pwmValue = map(config.sistema_brilho, 0, 100, 0, 255);
  analogWrite(TFT_BL, pwmValue);
#endif
}

void carregar_configuracoes() {
  EEPROM.begin(512);
  Config temporario;
  EEPROM.get(0, temporario);

  if (temporario.assinatura == 0x5A) {
    config = temporario;
  } else {
    config.assinatura = 0x5A;
    config.ss_selecionado = SS_TODOS;
    config.vel_gradiente = 5;
    config.qtd_asteroides = 20;
    config.intensidade_cores = 80;
    config.envio_ctrl_shift = true;
    config.prod_intervalo = 10;
    config.prod_inicio = 2;
    config.sistema_brilho = 100;

    // Odômetro
    config.odom_total_vida = 0;
    config.odom_teclas_hoje = 0;
    config.odom_data_hoje = 0;  // ← novo
    for (int i = 0; i < 7; i++) {
      config.historico_dias[i] = 0;
      config.historico_datas[i] = 0;
    }

    salvar_configuracoes();
  }
}

void salvar_configuracoes() {
  // ATENCAO (RP2040/RP2350 + Pico-PIO-USB): gravar na flash com o USB Host
  // ativo no core1 trava a placa. Por isso pausamos o core1 de forma
  // cooperativa antes de tocar na flash e o liberamos logo em seguida.
  g_flash_pausar_usb = true;
  uint32_t t0 = millis();
  while (!g_flash_usb_parado && (millis() - t0 < 100)) { /* aguarda core1 parar */
  }

  EEPROM.put(0, config);
  EEPROM.commit();

  g_flash_pausar_usb = false;
}


// Retorna 'true' apenas NO MOMENTO EM QUE O BOTÃO É PRESSIONADO (Borda de descida)
bool foiPressionado(ButtonDebounce& btn) {
  bool reading = digitalRead(btn.pin);

  // Se o estado do pino mudou em relação à última leitura, reseta o tempo
  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();
  }

  bool disparo = false;

  // Se a leitura permaneceu estável pelo tempo configurado
  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
    // Se o estado estável mudou de HIGH (solto) para LOW (pressionado)
    if (reading != btn.stableState) {
      btn.stableState = reading;
      if (btn.stableState == LOW) {
        disparo = true;  // Detectou um clique legítimo!
      }
    }
  }

  btn.lastState = reading;
  return disparo;
}

/*void tratar_botoes() {
  if ((millis() - last_debounce_time) < DEBOUNCE_DELAY) return;

  if (digitalRead(JOY_UP) == LOW) {
    processar_acao('U');
    last_debounce_time = millis();
  } else if (digitalRead(JOY_DOWN) == LOW) {
    processar_acao('D');
    last_debounce_time = millis();
  } else if (digitalRead(JOY_LEFT) == LOW) {
    processar_acao('B');
    last_debounce_time = millis();
  }  // LEFT = Voltar
  else if (digitalRead(JOY_RIGHT) == LOW) {
    processar_acao('S');
    last_debounce_time = millis();
  }  // RIGHT = Selecionar
  else if (digitalRead(JOY_OK) == LOW) {
    processar_acao('S');
    last_debounce_time = millis();
  }  // OK = Selecionar
}
*/

void tratar_botoes() {
  if (foiPressionado(btnUp)) {
    processar_acao('U');
  } else if (foiPressionado(btnDown)) {
    processar_acao('D');
  } else if (foiPressionado(btnLeft)) {
    processar_acao('B');  // LEFT = Voltar / Back
  } else if (foiPressionado(btnRight)) {
    processar_acao('S');  // RIGHT = Selecionar
  } else if (foiPressionado(btnOk)) {
    processar_acao('S');  // OK = Selecionar
  }
}

void processar_acao(char botao) {
  // --- MODO EDIÇÃO DE VALOR (Modificando Brilho, Velocidade, etc.) ---
  if (is_editing_value) {
    switch (botao) {
      case 'B':

      case 'S':
        is_editing_value = false;
        salvar_configuracoes();
        break;

      case 'U':
        ajustar_valor_variavel(1);
        break;

      case 'D':
        ajustar_valor_variavel(-1);
        break;
    }
    desenhar_menu();
    canvas.pushSprite(0, 0);
    return;
  }  // <-- ESSA CHAVE FECHA O IF (is_editing_value). O ERRO ESTAVA DAQUI PARA BAIXO

  // --- MODO NAVEGAÇÃO NORMAL (Andando pelos itens do Menu) ---
  int limite_itens = 0;
  switch (current_menu_level) {
    case LEVEL_MAIN: limite_itens = MAIN_MENU_COUNT; break;
    case LEVEL_SUB_SS: limite_itens = SS_MENU_COUNT; break;
    case LEVEL_SUB_CONFIG: limite_itens = CONFIG_MENU_COUNT; break;
    case LEVEL_SUB_PROD: limite_itens = PROD_MENU_COUNT; break;
    case LEVEL_SUB_SISTEMA: limite_itens = SISTEMA_MENU_COUNT; break;
    case LEVEL_SUB_ODOMETRO: limite_itens = 1; break;  // <-- Adicionado para o Odômetro
  }

  // Executa os comandos de navegação respeitando o limite do menu atual
  if (botao == 'U') {
    current_item_index = (current_item_index <= 0) ? limite_itens - 1 : current_item_index - 1;
  } else if (botao == 'D') {
    current_item_index = (current_item_index >= limite_itens - 1) ? 0 : current_item_index + 1;
  } else if (botao == 'B') {
    if (current_menu_level != LEVEL_MAIN) {
      if (current_menu_level == LEVEL_SUB_ODOMETRO) {
        current_item_index = 4;  // Volta apontando para o item Odômetro
      } else {
        current_item_index = 0;
      }
      current_menu_level = LEVEL_MAIN;
    }
  } else if (botao == 'S') {
    executar_selecao();
  }

  // Atualiza o display após a ação de navegação
  desenhar_menu();
  canvas.pushSprite(0, 0);
}

/*
void desenhar_menu_odometro_expandido() {
  // 1. Limpa o canvas do menu em segundo plano (sem flicker)
  canvas.fillSprite(TFT_BLACK);

  // 2. Cabeçalho estilizado
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("ESTATÍSTICAS DO ODÔMETRO", 160, 12);
  canvas.drawLine(0, 24, 320, 24, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);

  // -------------------------------------------------------------------
  // LADO ESQUERDO: HODÔMETRO CUSTOMIZADO DIRETO NO CANVAS DO MENU
  // -------------------------------------------------------------------
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString("TOTAL ACUMULADO:", 12, 35);

  // Renderização local do tambor do odômetro (7 dígitos fixos)
  char buf_total[16];
  sprintf(buf_total, "%07lu", odom_total_vida);  // Garante os zeros à esquerda

  int x_inicial = 12;
  int y_centro = 85;
  int boxW = 24;
  int boxH = 40;
  int gap = 3;
  int y0 = y_centro - boxH / 2;

  // Cores do tambor baseadas no volume de teclas
  uint16_t corDig = (odom_total_vida < 1000) ? TFT_GREEN : (odom_total_vida < 5000) ? TFT_YELLOW
                                                                                    : TFT_RED;
  uint16_t corFundo = tft.color565(15, 15, 15);
  uint16_t corBorda = tft.color565(70, 70, 70);

  canvas.setFreeFont(&JetBrainsMono_SemiBold20pt7b);
  canvas.setTextDatum(MC_DATUM);

  for (int i = 0; i < 7; i++) {
    int bx = x_inicial + i * (boxW + gap);
    // Desenha as caixinhas dos números
    canvas.fillRoundRect(bx, y0, boxW, boxH, 4, corFundo);
    canvas.drawRoundRect(bx, y0, boxW, boxH, 4, corBorda);
    // Linha central de efeito óptico do tambor
    canvas.drawFastHLine(bx + 2, y0 + boxH / 2, boxW - 4, tft.color565(35, 35, 35));

    // Plota o dígito correspondente
    char d[2] = { buf_total[i], '\0' };
    canvas.setTextColor(corDig, corFundo);
    canvas.drawString(d, bx + boxW / 2, y0 + boxH / 2 + 1);
  }
  canvas.setTextDatum(TL_DATUM);  // Restaura o alinhamento padrão

  // -------------------------------------------------------------------
  // DIVISOR VERTICAL SUTIL
  // -------------------------------------------------------------------
  canvas.drawFastVLine(205, 32, 110, tft.color565(50, 50, 50));

  // -------------------------------------------------------------------
  // LADO DIREITO: TEXTOS E MÉDIAS
  // -------------------------------------------------------------------
  canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);

  // Bloco 1: Hoje
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.drawString("DIGITADAS HOJE", 218, 38);

  //canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf_hoje[16];
  sprintf(buf_hoje, "%lu teclas", odom_teclas_hoje);
  canvas.drawString(buf_hoje, 218, 53);

  // Bloco 2: Média Móvel (Baseado no histórico do config)
  uint32_t soma_historico = 0;
  int dias_com_dados = 0;
  for (int i = 0; i < 7; i++) {
    if (config.historico_dias[i] > 0) {
      soma_historico += config.historico_dias[i];
      dias_com_dados++;
    }
  }
  uint32_t media_diaria = (dias_com_dados > 0) ? (soma_historico / dias_com_dados) : 0;

  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.drawString("MEDIA", 218, 85);

  canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf_media[16];
  sprintf(buf_media, "%lu /dia", media_diaria);
  canvas.drawString(buf_media, 218, 100);

  // -------------------------------------------------------------------
  // RODAPÉ DA TELA
  // -------------------------------------------------------------------
  canvas.drawLine(0, 146, 320, 146, tft.color565(40, 40, 40));
  canvas.setFreeFont(NULL);  // Fonte padrão para string simples
  canvas.setTextFont(1);
  canvas.setTextColor(TFT_SILVER, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("Pressione [Voltar/Esquerda] para sair", 160, 158);
  canvas.setTextDatum(TL_DATUM);

  // 3. O PULO DO GATO: Agora sim, um único push na tela inteira montada em RAM!
  canvas.pushSprite(0, 0);
}
*/

void desenhar_menu_odometro_expandido() {
  // 1. Limpa o canvas
  canvas.fillSprite(TFT_BLACK);

  // 2. Cabeçalho estilizado
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("ESTATISTICAS DO ODOMETRO", 160, 12);
  canvas.drawLine(0, 24, 320, 24, TFT_DARKGREY);
  canvas.setTextDatum(TL_DATUM);

  // -------------------------------------------------------------------
  // BLOCO 1: TAMBOR DO ODÔMETRO (Canto Superior Esquerdo)
  // -------------------------------------------------------------------
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString("TOTAL ACUMULADO:", 10, 32);

  char buf_total[16];
  sprintf(buf_total, "%07lu", odom_total_vida);

  int x_inicial = 10;
  int y_centro = 62;
  int boxW = 20;
  int boxH = 32;
  int gap = 2;
  int y0 = y_centro - boxH / 2;

  uint16_t corDig = (odom_total_vida < 1000) ? TFT_GREEN : (odom_total_vida < 5000) ? TFT_YELLOW : TFT_RED;
  uint16_t corFundo = tft.color565(15, 15, 15);
  uint16_t corBorda = tft.color565(70, 70, 70);

  canvas.setFreeFont(&JetBrainsMono_SemiBold20pt7b);
  canvas.setTextDatum(MC_DATUM);

  for (int i = 0; i < 7; i++) {
    int bx = x_inicial + i * (boxW + gap);
    canvas.fillRoundRect(bx, y0, boxW, boxH, 3, corFundo);
    canvas.drawRoundRect(bx, y0, boxW, boxH, 3, corBorda);
    canvas.drawFastHLine(bx + 2, y0 + boxH / 2, boxW - 4, tft.color565(35, 35, 35));

    char d[2] = { buf_total[i], '\0' };
    canvas.setTextColor(corDig, corFundo);
    canvas.drawString(d, bx + boxW / 2, y0 + boxH / 2 + 1);
  }
  canvas.setTextDatum(TL_DATUM);

  // -------------------------------------------------------------------
  // BLOCO 2: INFORMAÇÕES DE HOJE E MÉDIA (Canto Inferior Esquerdo)
  // -------------------------------------------------------------------
  canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);

  // Hoje
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.drawString("DIGITADAS HOJE:", 10, 88);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf_hoje[16];
  sprintf(buf_hoje, "%lu", odom_teclas_hoje);
  canvas.drawString(buf_hoje, 10, 100);

  // Média
  uint32_t soma_historico = 0;
  int dias_com_dados = 0;
  for (int i = 0; i < 7; i++) {
    if (config.historico_dias[i] > 0) {
      soma_historico += config.historico_dias[i];
      dias_com_dados++;
    }
  }
  uint32_t media_diaria = (dias_com_dados > 0) ? (soma_historico / dias_com_dados) : 0;

  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.drawString("MEDIA DIARIA:", 10, 116);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf_media[16];
  sprintf(buf_media, "%lu /dia", media_diaria);
  canvas.drawString(buf_media, 10, 128);

  // Divisor vertical separando texto do gráfico
  canvas.drawFastVLine(168, 30, 110, tft.color565(50, 50, 50));

  // -------------------------------------------------------------------
  // BLOCO 3: HISTOGRAMA DE 7 DIAS (Lado Direito)
  // -------------------------------------------------------------------
  // Chama o gráfico com offset no eixo X (posicionado no lado direito)
  desenhar_grafico_historico_ajustado(&canvas, config.historico_dias, 178, 132);

  // -------------------------------------------------------------------
  // RODAPÉ DA TELA
  // -------------------------------------------------------------------
  canvas.drawLine(0, 146, 320, 146, tft.color565(40, 40, 40));
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextColor(TFT_SILVER, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("Pressione [Voltar/Esquerda] para sair", 160, 158);
  canvas.setTextDatum(TL_DATUM);

  // Envia a imagem montada para o display
  canvas.pushSprite(0, 0);
}

void executar_selecao() {
  bool redraw = true;

  switch (current_menu_level) {
    case LEVEL_MAIN:
      if (current_item_index == 0) current_menu_level = LEVEL_SUB_SS;
      else if (current_item_index == 1) current_menu_level = LEVEL_SUB_CONFIG;
      else if (current_item_index == 2) current_menu_level = LEVEL_SUB_PROD;
      else if (current_item_index == 3) current_menu_level = LEVEL_SUB_SISTEMA;
      else if (current_item_index == 4) {  // <-- NOVO: Entrar no Odômetro
        current_menu_level = LEVEL_SUB_ODOMETRO;
        current_item_index = 0;
        if (redraw) {
          desenhar_menu();
          canvas.pushSprite(0, 0);
        }
        return;
      } else if (current_item_index == 5) {  // <-- MODIFICADO: Sair do menu principal (era 4)
        g_em_modo_menu = false;
        current_menu_level = LEVEL_MAIN;
        current_item_index = 0;
        desalocar_sprite_menu();
        firstDrawAfterSS = true;
        g_display_dirty = true;

        unsigned long timeout = millis() + 800;
        while ((digitalRead(JOY_OK) == LOW) && millis() < timeout) delay(10);
        return;
      }
      current_item_index = 0;
      break;

    case LEVEL_SUB_ODOMETRO:  // <-- NOVO: Se clicar com botão de Selecionar dentro do Odômetro, volta
      current_menu_level = LEVEL_MAIN;
      current_item_index = 4;  // Volta apontando para o item "Odometro"
      current_item_index = 0;  // Opcional, ou manter o 4 para foco estável
      break;

    case LEVEL_SUB_SS:
      if (current_item_index == SS_MENU_COUNT - 1) {  // "Voltar"
        current_menu_level = LEVEL_MAIN;
      } else {
        config.ss_selecionado = current_item_index;
        salvar_configuracoes();
      }
      current_item_index = 0;
      break;

    case LEVEL_SUB_CONFIG:
      if (current_item_index == CONFIG_MENU_COUNT - 1) {  // "Voltar"
        current_menu_level = LEVEL_MAIN;
      } else {
        is_editing_value = true;
      }
      current_item_index = 0;
      break;

    case LEVEL_SUB_PROD:
      if (current_item_index == PROD_MENU_COUNT - 1) {  // "Voltar"
        current_menu_level = LEVEL_MAIN;
      } else if (current_item_index == 0) {
        config.envio_ctrl_shift = !config.envio_ctrl_shift;
        salvar_configuracoes();
      } else {
        is_editing_value = true;
      }
      current_item_index = 0;
      break;

    case LEVEL_SUB_SISTEMA:
      if (current_item_index == SISTEMA_MENU_COUNT - 1) {  // "Voltar"
        current_menu_level = LEVEL_MAIN;
      } else if (current_item_index == 0) {
        is_editing_value = true;  // Brilho
      } else if (current_item_index == 1) {
        salvar_configuracoes();
        executar_reset_placa();
        return;
      }
      current_item_index = 0;
      break;
  }

  if (redraw) {
    desenhar_menu();
    canvas.pushSprite(0, 0);
  }
}

void ajustar_valor_variavel(int direcao) {
  if (current_menu_level == LEVEL_SUB_CONFIG) {
    if (current_item_index == 0) config.vel_gradiente = constrain(config.vel_gradiente + direcao, 1, 20);
    else if (current_item_index == 1) config.qtd_asteroides = constrain(config.qtd_asteroides + direcao, 0, 100);
    else if (current_item_index == 2) config.intensidade_cores = constrain(config.intensidade_cores + direcao, 0, 100);
  } else if (current_menu_level == LEVEL_SUB_PROD) {
    if (current_item_index == 1) config.prod_intervalo = constrain(config.prod_intervalo + direcao, 1, 60);
    else if (current_item_index == 2) config.prod_inicio = constrain(config.prod_inicio + direcao, 0, 60);
  } else if (current_menu_level == LEVEL_SUB_SISTEMA) {
    if (current_item_index == 0) {
      config.sistema_brilho = constrain(config.sistema_brilho + (direcao * 10), 0, 100);
      atualizarBrilho();  // ← ADICIONE ESTA LINHA
    }
  }
}

void desenhar_menu() {

  // --- REDIMENSIONAMENTO E LIMPEZA ---
  canvas.deleteSprite();
  canvas.createSprite(320, 172);  // Preenche a tela total (1.47" - 320x172)
  canvas.fillSprite(TFT_BLACK);

  const char* titulo = "MENU";
  int total_itens = 0;
  const char** itens_ponteiro = NULL;

  switch (current_menu_level) {
    case LEVEL_MAIN:
      titulo = "CONFIGURACOES";
      total_itens = MAIN_MENU_COUNT;
      itens_ponteiro = main_menu_items;
      break;
    case LEVEL_SUB_SS:
      titulo = "SCREENSAVER";
      total_itens = SS_MENU_COUNT;
      itens_ponteiro = ss_menu_items;
      break;
    case LEVEL_SUB_CONFIG:
      titulo = "AJUSTES";
      total_itens = CONFIG_MENU_COUNT;
      itens_ponteiro = config_menu_items;
      break;
    case LEVEL_SUB_PROD:
      titulo = "PRODUTIVIDADE";
      total_itens = PROD_MENU_COUNT;
      itens_ponteiro = prod_menu_items;
      break;
    case LEVEL_SUB_SISTEMA:
      titulo = "SISTEMA";
      total_itens = SISTEMA_MENU_COUNT;
      itens_ponteiro = sistema_menu_items;
      break;
    case LEVEL_SUB_ODOMETRO:
      desenhar_menu_odometro_expandido();
      return;
  }

  // --- CABEÇALHO DO MENU ---
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.setTextDatum(TL_DATUM);
  canvas.drawString(titulo, 10, 6);
  canvas.drawLine(0, 22, 320, 22, TFT_DARKGREY);

  // --- CÁLCULO DE PAGINAÇÃO / SCROLL ---
  int max_visiveis = 4;
  int item_inicial = 0;
  if (current_item_index >= max_visiveis) {
    item_inicial = current_item_index - max_visiveis + 1;
  }

  int y_pos = 30;         // Posição Y inicial do primeiro item
  int linha_altura = 30;  // Espaçamento entre itens para fonte maior

  // --- RENDERIZAÇÃO DOS ITENS ---
  for (int i = item_inicial; i < min(item_inicial + max_visiveis, total_itens); i++) {
    bool is_selected = (i == current_item_index);

    // Destaque do item selecionado (Barra na largura total da tela)
    if (is_selected) {
      uint16_t cor_fundo = is_editing_value ? TFT_RED : tft.color565(0, 80, 180);
      canvas.fillRoundRect(6, y_pos, 308, 24, 4, cor_fundo);
      canvas.setTextColor(TFT_WHITE, cor_fundo);
    } else {
      canvas.setTextColor(TFT_SILVER, TFT_BLACK);
    }

    // Texto do Item
    canvas.setFreeFont(&Orbitron_Bold6pt7b);
    canvas.setTextDatum(ML_DATUM);  // Alinhamento no meio-esquerdo
    canvas.drawString(itens_ponteiro[i], 18, y_pos + 12);

    // --- RENDERIZAÇÃO DOS VALORES E CHECKBOXES (LADO DIREITO) ---
    char val_buf[16] = "";

    if (current_menu_level == LEVEL_SUB_SS) {
      // Radio button para Seleção de Screensaver
      int radioX = 295;
      int radioY = y_pos + 12;
      canvas.drawCircle(radioX, radioY, 6, is_selected ? TFT_WHITE : TFT_DARKGREY);
      if (config.ss_selecionado == i) {
        canvas.fillCircle(radioX, radioY, 3, is_selected ? TFT_YELLOW : TFT_GREEN);
      }

    } else if (current_menu_level == LEVEL_SUB_CONFIG) {
      if (i == 0) sprintf(val_buf, "%d", config.vel_gradiente);
      else if (i == 1) sprintf(val_buf, "%d", config.qtd_asteroides);
      else if (i == 2) sprintf(val_buf, "%d", config.intensidade_cores);

      if (val_buf[0] != '\0') {
        canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
        canvas.setTextDatum(MR_DATUM);
        canvas.drawString(val_buf, 300, y_pos + 12);
      }

    } else if (current_menu_level == LEVEL_SUB_PROD) {
      if (i == 0) {
        // Toggle Siwth/Check
        int radioX = 295;
        int radioY = y_pos + 12;
        canvas.drawCircle(radioX, radioY, 6, is_selected ? TFT_WHITE : TFT_DARKGREY);
        if (config.envio_ctrl_shift) {
          canvas.fillCircle(radioX, radioY, 3, is_selected ? TFT_YELLOW : TFT_GREEN);
        }
      } else if (i == 1 || i == 2) {
        sprintf(val_buf, "%d min", (i == 1) ? config.prod_intervalo : config.prod_inicio);
        canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
        canvas.setTextDatum(MR_DATUM);
        canvas.drawString(val_buf, 300, y_pos + 12);
      }

    } else if (current_menu_level == LEVEL_SUB_SISTEMA) {
      if (i == 0) {
        sprintf(val_buf, "%d%%", config.sistema_brilho);
        canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
        canvas.setTextDatum(MR_DATUM);
        canvas.drawString(val_buf, 300, y_pos + 12);
      }
    }

    y_pos += linha_altura;
  }

  // Restaura o alinhamento padrão
  canvas.setTextDatum(TL_DATUM);

  // Envia o frame finalizado para o display
  canvas.pushSprite(0, 0);
}

void executar_reset_placa() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_RED);
  canvas.drawString("REINICIANDO...", 35, 35);
  canvas.pushSprite(0, 0);
  delay(1000);  // Dá tempo para o usuário ler a mensagem na tela

  // Executa o reboot nativo de hardware do RP2040/RP2350
  // Parâmetros: (tempo de delay antes do boot em ms, imagem de boot, fonte)
  watchdog_reboot(0, 0, 0);
}
