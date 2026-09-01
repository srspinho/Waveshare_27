#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// ==================== PINOS DO JOYSTICK ====================
#define JOY_COMMON   7     // Pino comum do joystick (geralmente ligado ao GND)

// Direções
#define JOY_UP       3
#define JOY_DOWN     2
#define JOY_LEFT     6
#define JOY_RIGHT    5
#define JOY_OK       4     // Botão central (SELECT)

// --- ENUMS DO MENU ---
enum MenuLevel { 
  LEVEL_MAIN, 
  LEVEL_SUB_SS, 
  LEVEL_SUB_CONFIG, 
  LEVEL_SUB_PROD, 
  LEVEL_SUB_SISTEMA, 
  LEVEL_SUB_ODOMETRO // <-- Adicionado para a tela de relatório
};

enum ScreensaverType { 
  SS_ATARI, SS_PONG, SS_ASTEROIDS, SS_STARFIELD, SS_GOL, 
  SS_MATRIX, SS_PACMAN, SS_FIREWORKS, SS_AQUARIUM, SS_TODOS 
};

// --- Estrutura para Cálculo de WPM (Palavras por Minuto) ---
struct WpmTracker {
    uint32_t ultimos_ms = 0;
    uint32_t contagem_janela = 0;
    uint16_t wpm_atual = 0;
};

extern WpmTracker g_wpm;

// --- Protótipos de Funções ---
void desenhar_grafico_historico(TFT_eSprite *canvas, const uint32_t historico[7]);
void atualizar_calculo_wpm(uint32_t total_teclas_atual);

// --- ESTRUTURA DE CONFIGURAÇÃO ---
// Substitua ou adicione estes campos no fim da sua "struct Config" no menu.h:
struct Config {
  uint8_t assinatura;
  uint8_t ss_selecionado;
  int vel_gradiente;
  int qtd_asteroides;
  int intensidade_cores;
  bool envio_ctrl_shift;
  int prod_intervalo;
  int prod_inicio;
  int sistema_brilho;
  // --- NOVOS CAMPOS PARA O ODÔMETRO ---
  uint32_t odom_total_vida;      // Contador acumulado da vida do teclado
  uint32_t odom_teclas_hoje;     // Contador do dia atual
  uint32_t odom_data_hoje;       // Data (AAAAMMDD) a que o contador "hoje" se refere
  uint32_t historico_dias[7];    // Guarda as teclas dos últimos 7 dias
  uint32_t historico_datas[7];   // Armazena a data compactada (AAAAMMDD) correspondente ao histórico
};

// Variáveis externas
extern Config config;
extern bool is_editing_value;
extern MenuLevel current_menu_level;
extern int current_item_index;
extern bool g_em_modo_menu;
extern void desalocar_sprite_menu();
extern bool firstDrawAfterSS;
extern volatile bool g_display_dirty;

// Handshake para pausar o USB Host (core1) durante gravacoes na flash.
// Gravar na flash com o Pico-PIO-USB ativo no core1 congela a placa.
extern volatile bool g_flash_pausar_usb;   // core0 pede para o core1 pausar
extern volatile bool g_flash_usb_parado;   // core1 confirma que pausou

// Protótipos
void setup_menu();
void tratar_botoes();
void desenhar_menu();
void carregar_configuracoes();
void salvar_configuracoes();

#endif