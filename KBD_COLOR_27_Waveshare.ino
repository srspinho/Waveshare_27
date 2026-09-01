/*********************************************************************
 * PROJETO: HID Remapper & System Monitor v2.5 + Pac-Man SS & Menu
 * HARDWARE: RP2040/RP2350 + TFT 160x80 (ST7735) -> Adaptado p/ 320x172
 *********************************************************************/

#include "usbh_helper.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include "RobotoMono12pt7b.h"
#include "Orbitron_Bold6pt7b.h"
#include "JetBrainsMono_SemiBold20pt7b.h"
#include "JetBrainsMono_Bold6pt7b.h"
#include "atari.h"
#include "menu.h"    // Integração com a persistência e UI do menu
#include <Wire.h>    // Adicionado para comunicação I2C
#include <RTClib.h>  // Biblioteca do RTC DS3231

// --- VARIÁVEIS EXTERNAS DO MENU ---
extern MenuLevel current_menu_level;
extern int current_item_index;

/* --- PROTÓTIPOS --- */
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);
void process_kbd_report(hid_keyboard_report_t const *report);
void remap_key(hid_keyboard_report_t const *original, hid_keyboard_report_t *remapped);
void update_display();

// Screensavers
void init_game();
void update_game();
void draw_game();

void init_matrix();
void update_matrix();
void draw_matrix();

void init_starfield();
void update_starfield();
void draw_starfield();

void init_pong();
void update_pong();
void draw_pong();

void init_pacman();
void update_pacman();
void draw_pacman_ss();
void drawPacManShape(int x, int y, int mouthAngle);
void drawGhostShape(int x, int y, uint16_t color, bool vulnerable);

void enviarAntiLock();
void init_bouncingball();
void update_bouncingball();
void init_fireworks();
void update_fireworks();
void draw_fireworks();
void init_aquarium();
void update_aquarium();
void draw_aquarium();

void init_asteroids();
void update_asteroids();
void draw_asteroids();
void init_atari();
void update_atari();
void draw_atari_logo();
uint16_t rainbowColor(int pos);
uint16_t neonPulseColor(int pos, float intensity, int phase);

char global_buf_data[12] = "";
char global_buf_hora[12] = "";

/* --- VARIÁVEIS DO ODÔMETRO (DS3231) --- */
RTC_DS3231 rtc;
uint32_t odom_total_vida = 0;
uint32_t odom_teclas_hoje = 0;

static int ultimo_minuto = -1;
static int ultimo_dia = -1;

unsigned long ultima_gravacao_flash = 0;
const unsigned long INTERVALO_GRAVACAO = 10 * 60 * 1000;

/* --- VARIÁVEIS PONG --- */
float ballX, ballY, ballDX, ballDY;
int paddle1Y, paddle2Y;
int scoreL = 0, scoreR = 0;
const int paddleH = 15;
const int paddleW = 3;
bool sprite_initialized = false;

int color_offset = 0;
int rainbow_offset = 0;
const int logoWidth = 74;
const int logoHeight = 80;

/* --- HARDWARE --- */
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

/* --- VARIÁVEIS GLOBAIS --- */
volatile uint32_t g_key_count = 0;
volatile uint8_t g_volume = 70;
volatile bool g_display_dirty = true;
unsigned long last_activity_time = 0;
unsigned long teams_alert_timeout = 0;
unsigned long lastGraphUpdate = 0;
bool title_drawn = false;
int lastVolume = -1;
unsigned long lastAntiLockTime = 0;

const uint32_t SS_SWITCH_INTERVAL = 30000;
unsigned long last_ss_switch = 0;
bool is_dimmed = false;
bool is_screensaver = false;
int current_ss_type = 0;
const uint32_t DIM_TIMEOUT = 30000;
const uint32_t LIFE_TIMEOUT = 120000;

bool g_em_modo_menu = false;

WpmTracker g_wpm;
static uint32_t ultimas_teclas_total = 0;

// Variáveis Pac-Man
int pac_posX = -60;
bool pac_initialized = false;
int prev_pac_posX = -60;

// Game of Life Vars
#define GOL_W 160
#define GOL_H 75
uint8_t world[GOL_W][GOL_H];
uint8_t next_world[GOL_W][GOL_H];
uint8_t world_last[GOL_W][GOL_H];
uint32_t history_checksum[4];
uint32_t last_gen_time = 0;
int stable_count = 0;

#define MATRIX_COLS_NEW 53
static int16_t matrix_y_new[MATRIX_COLS_NEW];
static uint8_t matrix_speed_new[MATRIX_COLS_NEW];

#define MAX_FISH 6
#define MAX_SEAHORSES 1
#define MAX_BUBBLES 12

/* --- VARIÁVEIS BOUNCING BALL --- */
float ballX2 = 160, ballY2 = 86;
float ballDX2 = 2.2, ballDY2 = 1.8;

/* ==================== SCREENSAVER: RANDOM EXPLOSIONS ==================== */
#define MAX_PARTICLES 60

struct Particle {
  float x, y;
  float vx, vy;
  uint8_t life;
  uint16_t color;
  bool trail;
};

Particle particles[MAX_PARTICLES];
unsigned long lastFirework = 0;

/* ==================== SCREENSAVER: BOUNCING BALLS (3 bolas) ==================== */
struct Ball {
  float x, y;
  float dx, dy;
  uint16_t color;
};

Ball balls[4];

// Starfield Vars
struct Star {
  int x;
  int y;
  int speed;
  uint16_t color;
  int size;
};
Star stars[50];

// Asteroids Vars
struct Asteroid {
  int x, y;
  int vx, vy;
  int size;
  int points;
  int angle[10];
  int radius[10];
};
#define MAX_ASTEROIDS 20
Asteroid asteroids[MAX_ASTEROIDS];

// ====================== ESTRUTURAS AQUARIO ======================
struct Fish {
  float x, y;
  float vx, vy;
  int type;
  bool active;
};

struct Seahorse {
  float x, y;
  float vx;
  bool facingRight;
  bool active;
};

struct Bubble {
  float x, y;
  float vy;
  bool active;
};

const char *fishGlyphs[] = { "><>", ">')>", "o><", "><" };
const int FISH_TYPES = 4;

Fish fishPool[MAX_FISH];
Seahorse seahorses[MAX_SEAHORSES];
Bubble bubbles[MAX_BUBBLES];

// Gráfico Vars
#define GRAPH_WIDTH 60
#define GRAPH_HEIGHT 20
int history[GRAPH_WIDTH] = { 0 };
int maxKeys = 20;
int contadorDeTeclas = 0;
bool graphNeedsUpdate = true;
//static char last_buf[12] = "";
//static int16_t last_xPos = 0;

// Matrix Vars
#define MATRIX_COLS 20
int8_t drop_pos[MATRIX_COLS];
uint8_t drop_speed[MATRIX_COLS];
uint32_t matrix_start_time = 0;
char matrix_last[MATRIX_COLS][8];

volatile uint8_t g_leds = 0;

// USB Vars
volatile uint8_t dev_addr_keyboard = 0;
volatile uint8_t instance_keyboard = 0;
volatile uint64_t keys_currently_pressed = 0;
#define FIFO_DISPLAY_UPDATE 1

uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(3))
};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_KEYBOARD, 2, false);
bool firstDrawAfterSS = true;

/* --- Handshake core0<->core1 para gravacoes seguras na flash --- */
// Gravar na flash (EEPROM.commit) com o Pico-PIO-USB ativo no core1 trava a
// placa. Antes de gravar, o core0 pausa o USB Host e espera esta confirmacao.
volatile bool g_flash_pausar_usb = false;  // core0 pede pausa
volatile bool g_flash_usb_parado = false;  // core1 confirma que parou

void atualizar_calculo_wpm(uint32_t total_teclas_atual) {
    uint32_t agora = millis();
    
    // Janela de amostragem de 10 segundos (10000 ms)
    if (agora - g_wpm.ultimos_ms >= 10000) {
        uint32_t teclas_no_intervalo = total_teclas_atual - ultimas_teclas_total;
        
        // Padrão da indústria: 1 palavra = 5 caracteres. 
        // WPM = (teclas / 5) * (60s / 10s) => teclas * 0.12 * 6 => teclas * 0.12
        g_wpm.wpm_atual = (uint16_t)((teclas_no_intervalo / 5.0) * 6.0);
        
        ultimas_teclas_total = total_teclas_atual;
        g_wpm.ultimos_ms = agora;
    }
}


/* ================================================================
   CORE 1: USB HOST
   ================================================================ */
void setup1() {
  rp2040_configure_pio_usb();
  USBHost.begin(1);
}

void loop1() {
  // Se o core0 vai gravar na flash, paramos de chamar USBHost.task() ate ser
  // liberado, evitando o congelamento causado por flash + Pico-PIO-USB.
  if (g_flash_pausar_usb) {
    g_flash_usb_parado = true;
    while (g_flash_pausar_usb) { /* aguarda o core0 terminar a gravacao */
    }
    g_flash_usb_parado = false;
    return;
  }
  USBHost.task();
}

void desalocar_sprite_menu() {
  canvas.deleteSprite();
  sprite_initialized = false;
  pac_initialized = false;
  canvas.createSprite(320, 172);
  canvas.fillSprite(TFT_BLACK);
}

/* ================================================================
   CORE 0: DISPLAY & LOGIC
   ================================================================ */
void setup() {
  Serial.begin(115200);
  usb_hid.setReportCallback(NULL, set_report_callback);
  usb_hid.begin();

  // Sincronização de Volume Nativa
  delay(3000);
  if (usb_hid.ready()) {
    for (int i = 0; i < 50; i++) {
      uint8_t vol_down[2] = { 0xEA, 0x00 };
      usb_hid.sendReport(2, vol_down, 2);
      delay(10);
      uint8_t release[2] = { 0x00, 0x00 };
      usb_hid.sendReport(2, release, 2);
      delay(10);
    }
    for (int i = 0; i < 35; i++) {
      uint8_t vol_up[2] = { 0xE9, 0x00 };
      usb_hid.sendReport(2, vol_up, 2);
      delay(10);
      uint8_t release[2] = { 0x00, 0x00 };
      usb_hid.sendReport(2, release, 2);
      delay(10);
    }
    g_volume = 70;
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Aloca o canvas globalmente de forma segura
  canvas.createSprite(320, 172);
  canvas.fillSprite(TFT_BLACK);

  randomSeed(analogRead(28) + micros());  // GP26/GP27 agora sao I2C; usa GP28 (ADC2) para a semente

  // RTC no barramento I2C1 (SDA=GP26, SCL=GP27).
  // ATENCAO: NAO usar GP4/GP5 aqui: sao os mesmos pinos do JOY_OK/JOY_RIGHT,
  // e o setup_menu() faz pinMode() neles, o que quebrava a leitura do RTC.
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  Wire1.begin();
  Wire1.setClock(100000);

  if (!rtc.begin(&Wire1)) {
    Serial.println("❌ [RTC] Erro: DS3231 nao foi encontrado!");
  } else {
    Serial.println(" [RTC] DS3231 inicializado com sucesso.");
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    DateTime agora = rtc.now();
    ultimo_minuto = agora.minute();
    ultimo_dia = agora.day();
  }

  //NOVA VERIFICAÇÃO DO
  DateTime agora = rtc.now();
  uint32_t data_atual_id = (agora.year() * 10000UL) + (agora.month() * 100) + agora.day();

  // Se a data salva for diferente da data de hoje → virada de dia ocorreu enquanto a placa estava desligada
  if (config.odom_data_hoje != data_atual_id) {
    // Move o valor antigo para o histórico (mesmo código que você já tem no loop)
    DateTime ontem = agora - TimeSpan(1, 0, 0, 0);
    uint32_t data_ontem_id = (ontem.year() * 10000UL) + (ontem.month() * 100) + ontem.day();

    for (int i = 6; i > 0; i--) {
      config.historico_dias[i] = config.historico_dias[i - 1];
      config.historico_datas[i] = config.historico_datas[i - 1];
    }
    config.historico_dias[0] = odom_teclas_hoje;  // ou config.odom_teclas_hoje
    config.historico_datas[0] = data_ontem_id;

    // Zera o contador do dia
    odom_teclas_hoje = 0;
    config.odom_teclas_hoje = 0;
    config.odom_data_hoje = data_atual_id;

    salvar_configuracoes();  // grava a nova data + zero + histórico
  }

  ultimo_dia = agora.day();  // continua útil para a detecção em tempo real

  setup_menu();

  // --- CORREÇÃO DA PERSISTÊNCIA E INICIALIZAÇÃO DO ODÔMETRO ---
  carregar_configuracoes();

  odom_total_vida = config.odom_total_vida;
  odom_teclas_hoje = config.odom_teclas_hoje;
  g_key_count = 0;

  // Verifica se a data salva do "hoje" ainda é a data atual.
  // Se a placa ficou desligada e ligou no dia seguinte, faz a virada aqui.
  if (rtc.begin(&Wire1)) {  // só se o RTC estiver ok
    DateTime agora = rtc.now();
    uint32_t data_atual_id = (uint32_t)agora.year() * 10000UL
                             + (uint32_t)agora.month() * 100UL
                             + (uint32_t)agora.day();

    if (config.odom_data_hoje != data_atual_id) {
      Serial.println(" [ODÔMETRO] Dia diferente detectado no boot → virada de dia");

      // Empurra o valor antigo para o histórico (mesmo algoritmo do loop)
      DateTime ontem = agora - TimeSpan(1, 0, 0, 0);
      uint32_t data_ontem_id = (uint32_t)ontem.year() * 10000UL
                               + (uint32_t)ontem.month() * 100UL
                               + (uint32_t)ontem.day();

      for (int i = 6; i > 0; i--) {
        config.historico_dias[i] = config.historico_dias[i - 1];
        config.historico_datas[i] = config.historico_datas[i - 1];
      }
      config.historico_dias[0] = odom_teclas_hoje;
      config.historico_datas[0] = data_ontem_id;

      // Zera o contador do dia e grava a nova data
      odom_teclas_hoje = 0;
      config.odom_teclas_hoje = 0;
      config.odom_data_hoje = data_atual_id;

      salvar_configuracoes();
    }

    ultimo_minuto = agora.minute();
    ultimo_dia = agora.day();
  }

  Serial.print(" [ODÔMETRO] Recuperado da Flash - Total: ");
  Serial.print(odom_total_vida);
  Serial.print(" | Hoje: ");
  Serial.println(odom_teclas_hoje);
  // --------------------------------------------------------------

#ifdef TFT_BL
  analogWrite(TFT_BL, map(config.sistema_brilho, 0, 100, 0, 255));
#endif

  g_volume = 70;

  if (config.prod_inicio == 0) {
    last_activity_time = millis() - (60ULL * 1000ULL);
  } else {
    last_activity_time = millis();
  }

  g_em_modo_menu = false;
  firstDrawAfterSS = true;
  g_display_dirty = true;

  pinMode(TFT_BL, OUTPUT);
  analogWriteFreq(10000);
  analogWriteRange(255);
  analogWrite(TFT_BL, config.sistema_brilho);
}

void loop() {
  uint32_t now = millis();

  // RTC: UMA unica leitura por segundo e NUNCA durante o screensaver.
  // rtc.now() e uma transacao I2C bloqueante. Le-lo dentro do laco de render
  // (e ainda por cima duas vezes por segundo) era o que deixava os screensavers lentos.
  // A leitura fica em cache e e reaproveitada pelo odometro (sem I2C extra).
  static unsigned long timing_rtc = 0;
  static DateTime agora_cache;
  static bool rtc_valido = false;
  if (!is_screensaver && (now - timing_rtc >= 1000)) {
    timing_rtc = now;
    agora_cache = rtc.now();
    // So aceita/exibe se a leitura for coerente. Assim uma eventual leitura I2C
    // corrompida nao gera valores absurdos (ex.: mes 16, minuto 143) na tela.
    if (agora_cache.year() >= 2000 && agora_cache.year() <= 2100 && agora_cache.month() >= 1 && agora_cache.month() <= 12 && agora_cache.day() >= 1 && agora_cache.day() <= 31 && agora_cache.hour() < 24 && agora_cache.minute() < 60 && agora_cache.second() < 60) {
      rtc_valido = true;
      sprintf(global_buf_data, "%02d/%02d/%02d", agora_cache.day(), agora_cache.month(), agora_cache.year() % 100);
      sprintf(global_buf_hora, "%02d:%02d:%02d", agora_cache.hour(), agora_cache.minute(), agora_cache.second());
    } else {
      rtc_valido = false;  // leitura incoerente: ignora este segundo (nao exibe nem contabiliza no odometro)
    }
  }


  /*if (dia_atual != ultimo_dia) {
    DateTime ontem = agora - TimeSpan(1, 0, 0, 0);
    uint32_t data_ontem_id = (ontem.year() * 10000) + (ontem.month() * 100) + ontem.day();

    for (int i = 6; i > 0; i--) {
      config.historico_dias[i] = config.historico_dias[i - 1];
      config.historico_datas[i] = config.historico_datas[i - 1];
    }

    config.historico_dias[0] = odom_teclas_hoje;
    config.historico_datas[0] = data_ontem_id;

    odom_teclas_hoje = 0;
    config.odom_teclas_hoje = 0;

    // NOVO: grava a data de hoje
    uint32_t data_atual_id = (uint32_t)agora.year() * 10000UL
                             + (uint32_t)agora.month() * 100UL
                             + (uint32_t)agora.day();
    config.odom_data_hoje = data_atual_id;

    ultimo_dia = dia_atual;

    extern void salvar_configuracoes();
    salvar_configuracoes();
  }
  */

  static bool menu_estava_ativo = false;

  if (g_em_modo_menu) {
    menu_estava_ativo = true;
    tratar_botoes();
    return;
  }

  if (menu_estava_ativo && !g_em_modo_menu) {
    menu_estava_ativo = false;
    desalocar_sprite_menu();
    firstDrawAfterSS = true;
    g_display_dirty = true;
    tft.fillScreen(TFT_BLACK);
  }

  // Entrada no menu por botões físicos
  if (!g_em_modo_menu && (digitalRead(JOY_UP) == LOW || digitalRead(JOY_DOWN) == LOW || digitalRead(JOY_OK) == LOW || digitalRead(JOY_LEFT) == LOW)) {
    static unsigned long lastMenuEntry = 0;
    if (now - lastMenuEntry < 800) return;

    g_em_modo_menu = true;
    current_menu_level = LEVEL_MAIN;
    current_item_index = 0;

    extern unsigned long last_debounce_time;
    last_debounce_time = now;

    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    desenhar_menu();
    canvas.pushSprite(0, 0);

    while (digitalRead(JOY_UP) == LOW || digitalRead(JOY_DOWN) == LOW || digitalRead(JOY_OK) == LOW || digitalRead(JOY_LEFT) == LOW) {
      delay(10);
    }

    lastMenuEntry = now;
    delay(150);
    return;
  }

  // Monitor de teclas
  static uint32_t last_key_val = 0;
  if (g_key_count != last_key_val) {
    last_key_val = g_key_count;
    last_activity_time = now;
    if (is_screensaver || is_dimmed) {
      is_screensaver = false;
      is_dimmed = false;
      firstDrawAfterSS = true;
      g_display_dirty = true;
      tft.fillScreen(TFT_BLACK);
    }
    contadorDeTeclas++;
    g_display_dirty = true;
  }

  // Anti-Lock
  if (config.envio_ctrl_shift) {
    uint32_t idleTime = now - last_activity_time;
    if (idleTime > 15000 && (now - lastAntiLockTime > 15000)) {
      enviarAntiLock();
      lastAntiLockTime = now;
    }
  }

  // Atualização de dados do Gráfico
  if (now - lastGraphUpdate >= 1000) {
    lastGraphUpdate = now;
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) history[i] = history[i + 1];
    history[GRAPH_WIDTH - 1] = contadorDeTeclas;
    contadorDeTeclas = 0;
    graphNeedsUpdate = true;
    g_display_dirty = true;
  }

  uint32_t idle_time = now - last_activity_time;
  unsigned long menu_life_timeout = (unsigned long)config.prod_inicio * 60 * 1000;

  // Lógica de Screensaver
  if (idle_time > menu_life_timeout) {
    if (!is_screensaver) {
      is_screensaver = true;
      tft.fillScreen(TFT_BLACK);

      // === ADICIONE ESTE BLOCO AQUI PARA SALVAR ANTES DE ENTRAR NO SS ===
      Serial.println(" [FLASH] Salvando odômetro antes de entrar no Screensaver...");
      config.odom_teclas_hoje = odom_teclas_hoje;
      config.odom_total_vida = odom_total_vida;
      extern void salvar_configuracoes();
      salvar_configuracoes();

      if (config.ss_selecionado == SS_TODOS) {
        current_ss_type = 0;
      } else {
        current_ss_type = config.ss_selecionado;
      }

      switch (current_ss_type) {
        case 0: init_atari(); break;
        case 1: init_bouncingball(); break;
        case 2: init_asteroids(); break;
        case 3: init_starfield(); break;
        case 4: init_game(); break;
        case 5: init_matrix(); break;
        case 6: init_pacman(); break;
        case 7: init_fireworks(); break;
        case 8: init_aquarium(); break;
        default: init_pacman(); break;
      }
      last_ss_switch = now;
    }

    if (config.ss_selecionado == SS_TODOS && (now - last_ss_switch > SS_SWITCH_INTERVAL)) {
      current_ss_type = (current_ss_type + 1) % 9;
      tft.fillScreen(TFT_BLACK);

      switch (current_ss_type) {
        case 0: init_atari(); break;
        case 1: init_bouncingball(); break;
        case 2: init_asteroids(); break;
        case 3: init_starfield(); break;
        case 4: init_game(); break;
        case 5: init_matrix(); break;
        case 6: init_pacman(); break;
        case 7: init_fireworks(); break;
        case 8: init_aquarium(); break;
      }
      last_ss_switch = now;
    }

    // Controle de framerate inteligente dependendo do screensaver
    uint32_t speed = map(config.vel_gradiente, 1, 20, 180, 20);
    if (current_ss_type == 0) speed = max(speed, 30u);       // Atari (acelerado)
    else if (current_ss_type == 1) speed = max(speed, 16u);  // Bouncing Ball
    else if (current_ss_type == 4) speed += 30;
    else if (current_ss_type == 5) speed += 40;

    if (now - last_gen_time > speed) {
      switch (current_ss_type) {
        case 0: update_atari(); break;
        case 1: update_bouncingball(); break;
        case 2: update_asteroids(); break;
        case 3: update_starfield(); break;
        case 4: update_game(); break;
        case 5: update_matrix(); break;
        case 6: update_pacman(); break;
        case 7: update_fireworks(); break;
        case 8: update_aquarium(); break;
      }
      g_display_dirty = true;
      last_gen_time = now;
    }

  } else {
    is_dimmed = (idle_time > DIM_TIMEOUT);
  }

  // Controle de odômetro (NAO roda durante o screensaver: evita leituras I2C e
  // gravacoes em flash enquanto o screensaver esta ativo).
static unsigned long ultimo_check_rtc = 0;
  if (!is_screensaver && rtc_valido && (now - ultimo_check_rtc >= 1000)) {
    ultimo_check_rtc = now;
    DateTime agora = agora_cache;          // reaproveita a leitura já feita
    int minuto_atual = agora.minute();
    int dia_atual = agora.day();

    if (g_key_count > 0) {
      uint32_t diff = g_key_count;
      g_key_count = 0;

      odom_teclas_hoje += diff;
      odom_total_vida  += diff;

      config.odom_teclas_hoje = odom_teclas_hoje;
      config.odom_total_vida  = odom_total_vida;
    }

    if (minuto_atual != ultimo_minuto) {
      ultimo_minuto = minuto_atual;
    }

    if (now - ultima_gravacao_flash >= INTERVALO_GRAVACAO) {
      ultima_gravacao_flash = now;
      if (config.odom_total_vida != odom_total_vida || config.odom_teclas_hoje != odom_teclas_hoje) {
        extern void salvar_configuracoes();
        salvar_configuracoes();
      }
    }

    // ===== VIRADA DE DIA (tem que ficar AQUI dentro) =====
    if (dia_atual != ultimo_dia) {
      DateTime ontem = agora - TimeSpan(1, 0, 0, 0);
      uint32_t data_ontem_id = (uint32_t)ontem.year() * 10000UL
                             + (uint32_t)ontem.month() * 100UL
                             + (uint32_t)ontem.day();

      for (int i = 6; i > 0; i--) {
        config.historico_dias[i]  = config.historico_dias[i - 1];
        config.historico_datas[i] = config.historico_datas[i - 1];
      }

      config.historico_dias[0]  = odom_teclas_hoje;
      config.historico_datas[0] = data_ontem_id;

      odom_teclas_hoje = 0;
      config.odom_teclas_hoje = 0;

      // Grava a data de hoje (novo campo)
      uint32_t data_atual_id = (uint32_t)agora.year() * 10000UL
                             + (uint32_t)agora.month() * 100UL
                             + (uint32_t)agora.day();
      config.odom_data_hoje = data_atual_id;

      ultimo_dia = dia_atual;

      extern void salvar_configuracoes();
      salvar_configuracoes();
    }
  }

  while (rp2040.fifo.available()) {
    if (rp2040.fifo.pop() == FIFO_DISPLAY_UPDATE) {
      g_display_dirty = true;
    }
  }

  // Atualiza a amostragem contínua de WPM
  atualizar_calculo_wpm(odom_teclas_hoje);

  if (g_display_dirty) {
    update_display();
    g_display_dirty = false;
  }
}

/* ==================== ODOMETRO DE TECLAS (estilo hodometro) ==================== */
// Desenha o total acumulado como um hodometro de carro: digitos com zeros a
// esquerda, cada um em sua "caixinha". Centralizado horizontalmente, em torno de cy.
void desenhar_odometro(uint32_t valor, int x0, int cy, int boxW, int boxH, bool darPush = true) {

  char buf[16];
  sprintf(buf, "%07lu", valor);  // no minimo 7 digitos, com zeros a esquerda
  int n = strlen(buf);

  const int gap = 3;
  int y0 = cy - boxH / 2;

  uint16_t corDig = (valor < 1000) ? TFT_GREEN : (valor < 5000) ? TFT_YELLOW
                                                                : TFT_RED;
  uint16_t corFundo = tft.color565(15, 15, 15);
  uint16_t corBorda = tft.color565(70, 70, 70);

  canvas.setFreeFont(&JetBrainsMono_SemiBold20pt7b);
  canvas.setTextDatum(MC_DATUM);

  for (int i = 0; i < n; i++) {
    int bx = x0 + i * (boxW + gap);
    canvas.fillRoundRect(bx, y0, boxW, boxH, 4, corFundo);
    canvas.drawRoundRect(bx, y0, boxW, boxH, 4, corBorda);
    // Linha central sutil, dando o ar de "tambor" do hodometro
    canvas.drawFastHLine(bx + 2, y0 + boxH / 2, boxW - 4, tft.color565(35, 35, 35));

    char d[2] = { buf[i], '\0' };
    canvas.setTextColor(corDig, corFundo);
    canvas.drawString(d, bx + boxW / 2, y0 + boxH / 2 + 1);
  }

  canvas.setTextDatum(TL_DATUM);  // restaura o padrao usado no resto da tela
                                  // ALTERAÇÃO AQUI: Só dá o push se for solicitado (na tela principal)
  //if (darPush) {
  //  canvas.pushSprite(0, 0);
  //}
}

void update_display() {
  if (g_em_modo_menu) return;

  if (is_screensaver) {
    switch (current_ss_type) {
      case 0: draw_atari_logo(); break;
      case 1: /* update_bouncingball faz tudo via DMA interno */ break;
      case 2: draw_asteroids(); break;
      case 3: draw_starfield(); break;
      case 4: draw_game(); break;
      case 5: draw_matrix(); break;
      case 6: draw_pacman_ss(); break;
      case 7: draw_fireworks(); break;
      case 8: draw_aquarium(); break;
    }
    return;
  }

  // ====================== MODO MONITOR NORMAL ======================
  canvas.fillSprite(TFT_BLACK);

  if (firstDrawAfterSS) {
    tft.fillScreen(TFT_BLACK);
    firstDrawAfterSS = false;
    title_drawn = false;
    lastVolume = -1;
    graphNeedsUpdate = true;
  }

  // Titulo centralizado no topo
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("SYSTEM MONITOR v2.5 | RP2350", 160, 10);
  canvas.setTextDatum(TL_DATUM);
  canvas.drawLine(0, 20, 320, 20, TFT_DARKGREY);

// Rotulo do hodometro (centralizado sobre os digitos, a esquerda)
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("TECLAS DO DIA", 121, 33);
  canvas.setTextDatum(TL_DATUM);

  // Hodometro do total acumulado, alinhado a esquerda
  desenhar_odometro(odom_teclas_hoje, 4, 78, 31, 46);

  // Data / hora / total de hoje no lado direito, ao lado do hodometro
  if (global_buf_data[0] != '\0') {
    canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(TFT_SILVER, TFT_BLACK);
    canvas.drawString(global_buf_data, 312, 46);
    canvas.drawString(global_buf_hora, 312, 60);

    char buf_hoje[12];
    sprintf(buf_hoje, "%lu", odom_total_vida);
    canvas.setTextColor(TFT_SILVER, TFT_BLACK);
    canvas.drawString(buf_hoje, 312, 76);
    canvas.setTextDatum(TL_DATUM);
  }

  // =========================================================================
  // --- NOVAS FUNCIONALIDADES: WPM E INDICADOR TURBO ---
  // =========================================================================
// =========================================================================
  // --- BLOCO WPM E TURBO (Posicionado no lado direito, alinhado com data/hora) ---
  // =========================================================================
  canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
  canvas.setTextDatum(TR_DATUM); // Alinhamento a direita (igual data/hora)
  
  char buf_wpm[16];
  sprintf(buf_wpm, "WPM: %d", g_wpm.wpm_atual);
  
  // Desenha no lado direito (X=312), logo acima do bloco da data (Y=30)
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.drawString(buf_wpm, 312, 30);

  // Badge TURBO! (apenas quando ultrapassar 60 WPM)
  if (g_wpm.wpm_atual >= 60) {
    // Desenha ao lado do WPM, no topo direito
    canvas.fillRect(200, 26, 55, 14, TFT_RED);
    canvas.drawRect(200, 26, 55, 14, TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(TFT_WHITE, TFT_RED);
    canvas.drawString("TURBO!", 227, 33);
  }
  
  canvas.setTextDatum(TL_DATUM); // Restaura o alinhamento padrao
  // =========================================================================

  // Gráfico Otimizado (Escrito direto no canvas)
  canvas.fillRect(10, 110, 142, 40, tft.color565(20, 20, 20));
  canvas.drawRect(10, 110, 142, 40, TFT_WHITE);
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    int h1 = map(history[i], 0, maxKeys, 0, 36);
    int h2 = map(history[i + 1], 0, maxKeys, 0, 36);
    canvas.drawLine(15 + i * 2, 148 - h1, 15 + (i + 1) * 2, 148 - h2, TFT_GREEN);
  }

  // Volume renderizado no Canvas
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.drawString("VOLUME", 170, 110);
  canvas.drawRect(170, 126, 140, 14, TFT_WHITE);
  int barWidth = map(g_volume, 0, 100, 0, 134);
  canvas.fillRect(173, 129, barWidth, 8, (g_volume < 50) ? TFT_BLUE : TFT_GREEN);

  // Envia tudo de uma só vez para o display
  canvas.pushSprite(0, 0);
}

void init_aquarium() {
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);

  for (int i = 0; i < MAX_FISH; i++) {
    fishPool[i].x = random(5, 300);
    fishPool[i].y = random(15, 135);
    fishPool[i].vx = random(0, 2) ? random(5, 12) / 10.0f : -random(5, 12) / 10.0f;
    fishPool[i].vy = random(-5, 6) / 10.0f;
    fishPool[i].type = random(0, 4);
    fishPool[i].active = true;
  }

  for (int i = 0; i < MAX_SEAHORSES; i++) {
    seahorses[i].x = random(20, 290);
    seahorses[i].y = random(25, 120);
    seahorses[i].vx = random(0, 2) ? random(6, 12) / 10.0f : -random(6, 12) / 10.0f;
    seahorses[i].facingRight = (seahorses[i].vx > 0);
    seahorses[i].active = true;
  }

  for (int i = 0; i < MAX_BUBBLES; i++) {
    bubbles[i].x = random(10, 310);
    bubbles[i].y = random(15, 165);
    bubbles[i].vy = random(6, 14) / 10.0f;
    bubbles[i].active = true;
  }
}

void update_aquarium() {
  for (int i = 0; i < MAX_FISH; i++) {
    Fish &f = fishPool[i];
    if (!f.active) continue;
    f.x += f.vx;
    f.y += f.vy;
    if (f.x < -30) f.x = 325;
    if (f.x > 325) f.x = -30;
    if (f.y < 12) {
      f.y = 12;
      f.vy = 0.4f;
    }
    if (f.y > 135) {
      f.y = 135;
      f.vy = -0.4f;
    }
  }

  for (int i = 0; i < MAX_SEAHORSES; i++) {
    Seahorse &s = seahorses[i];
    if (!s.active) continue;
    s.x += s.vx;
    s.y += sin(millis() / 900.0f + i) * 0.4f;
    if (s.x < -40) {
      s.x = 325;
      s.facingRight = true;
      s.vx = abs(s.vx);
    }
    if (s.x > 325) {
      s.x = -40;
      s.facingRight = false;
      s.vx = -abs(s.vx);
    }
    if (s.y < 15) s.y = 15;
    if (s.y > 120) s.y = 120;
  }

  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (!bubbles[i].active) continue;
    bubbles[i].y -= bubbles[i].vy;
    bubbles[i].x += sin(millis() / 550.0f + i) * 0.35f;
    if (bubbles[i].y < -8) {
      bubbles[i].y = 168;
      bubbles[i].x = random(8, 312);
    }
  }
}

void draw_aquarium() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextSize(1);

  canvas.fillRect(0, 142, 320, 30, 0x0010);
  canvas.fillRect(0, 168, 320, 4, 0xbdcd);

  float t = millis() / 1100.0f;
  float sway1 = sin(t + 0.3f) * 3.0f;
  canvas.setTextColor(0x03E0);
  canvas.drawString(")", 20 + (int)sway1 * 0.5, 126);
  canvas.drawString("(", 19 + (int)sway1 * 1.0, 133);
  canvas.drawString("|", 18 + (int)sway1 * 1.5, 140);

  float sway2 = sin(t * 1.1f + 1.8f) * 2.5f;
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(")", 72 + (int)sway2 * 0.5, 122);
  canvas.drawString("(", 70 + (int)sway2 * 1.1, 129);
  canvas.drawString("|", 71 + (int)sway2 * 1.4, 136);

  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (bubbles[i].active) {
      if (i % 2 == 0) {
        canvas.drawPixel((int)bubbles[i].x, (int)bubbles[i].y, 0x7BEF);
      } else {
        canvas.drawCircle((int)bubbles[i].x, (int)bubbles[i].y, 1, TFT_WHITE);
      }
    }
  }

  for (int i = 0; i < MAX_FISH; i++) {
    if (fishPool[i].active) {
      canvas.setTextColor(TFT_CYAN);
      canvas.drawString(fishGlyphs[fishPool[i].type], (int)fishPool[i].x, (int)fishPool[i].y);
    }
  }

  for (int i = 0; i < MAX_SEAHORSES; i++) {
    if (seahorses[i].active) {
      canvas.setTextColor(TFT_MAGENTA);
      int sx = (int)seahorses[i].x;
      int sy = (int)seahorses[i].y;
      if (seahorses[i].facingRight) {
        // Desenho original olhando para a DIREITA (>)
        canvas.drawString("  ^^  ", sx, sy);
        canvas.drawString(" /o ) ", sx, sy + 6);
        canvas.drawString("[__-/ ", sx, sy + 12);
        canvas.drawString("  /|  ", sx, sy + 18);
        canvas.drawString(" / |  ", sx, sy + 24);
        canvas.drawString(" \\ | ", sx, sy + 30);
        canvas.drawString("  \\_/", sx, sy + 36);
      } else {
        // Desenho ESPELHADO olhando para a ESQUERDA (<)
        canvas.drawString("  ^^  ", sx, sy);
        canvas.drawString(" ( o\\ ", sx, sy + 6);
        canvas.drawString(" \\-__]", sx, sy + 12);
        canvas.drawString("  |\\  ", sx, sy + 18);
        canvas.drawString("  | \\ ", sx, sy + 24);
        canvas.drawString("  | / ", sx, sy + 30);
        canvas.drawString("  \\_/ ", sx, sy + 36);
      }
    }
  }
  canvas.pushSprite(0, 0);
}

void init_fireworks() {
  canvas.fillSprite(TFT_BLACK);
  for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0;
  lastFirework = 0;
}

void createFirework(int cx, int cy) {
  uint16_t colors[12] = {
    TFT_RED, TFT_YELLOW, TFT_WHITE, TFT_GREEN,
    tft.color565(255, 0, 128), tft.color565(0, 255, 255),
    tft.color565(255, 128, 0), tft.color565(128, 255, 0),
    tft.color565(200, 50, 255), tft.color565(255, 255, 100),
    tft.color565(100, 150, 255), tft.color565(255, 100, 100)
  };

  int explosionMode = random(0, 2);
  uint16_t color1 = colors[random(0, 12)];
  uint16_t color2 = colors[random(0, 12)];
  int numParticles = random(16, 26);

  for (int i = 0; i < numParticles; i++) {
    for (int j = 0; j < MAX_PARTICLES; j++) {
      if (particles[j].life == 0) {
        particles[j].x = cx;
        particles[j].y = cy;
        float angle = random(0, 360) * PI / 180.0;
        float speed = random(10, 26) / 10.0;
        particles[j].vx = cos(angle) * speed;
        particles[j].vy = sin(angle) * speed - random(2, 7) / 10.0;
        particles[j].life = random(35, 58);
        particles[j].color = (explosionMode == 0) ? colors[random(0, 12)] : ((random(0, 2) == 0) ? color1 : color2);
        if (random(0, 8) == 0) particles[j].color = TFT_WHITE;
        break;
      }
    }
  }
}

void update_fireworks() {
  static unsigned long last = 0;
  if (millis() - last < 20) return;
  last = millis();

  if (millis() - lastFirework > (unsigned long)random(800, 1800)) {
    createFirework(random(20, 300), random(15, 110));
    lastFirework = millis();
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      particles[i].vy += 0.07;
      particles[i].vx *= 0.96;
      particles[i].life--;
    }
  }
}

void draw_fireworks() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextSize(1);

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      int px = (int)particles[i].x;
      int py = (int)particles[i].y;
      if (px < 0 || px >= 320 || py < 0 || py >= 172) continue;

      uint16_t col = particles[i].color;
      if (particles[i].life > 40) {
        canvas.fillRect(px - 1, py - 1, 3, 3, TFT_WHITE);
      } else if (particles[i].life > 15) {
        canvas.fillRect(px, py, 2, 2, col);
      } else {
        canvas.drawPixel(px, py, col);
      }
    }
  }
  canvas.pushSprite(0, 0);
}

void init_bouncingball() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 3; i++) {
    balls[i].x = random(15, 305);
    balls[i].y = random(15, 155);
    balls[i].dx = (random(18, 28) / 10.0) * (random(0, 2) ? 1 : -1);
    balls[i].dy = (random(18, 28) / 10.0) * (random(0, 2) ? 1 : -1);
  }
}

void update_bouncingball() {
  // Agora usa o Canvas para desenhar as Bouncing Balls, o que elimina o uso de fillRect lento na tela física!
  canvas.fillSprite(TFT_BLACK);
  for (int i = 0; i < 3; i++) {
    balls[i].x += balls[i].dx;
    balls[i].y += balls[i].dy;

    if (balls[i].x < 7) {
      balls[i].x = 7;
      balls[i].dx *= -1.05;
    } else if (balls[i].x > 313) {
      balls[i].x = 313;
      balls[i].dx *= -1.05;
    }

    if (balls[i].y < 7) {
      balls[i].y = 7;
      balls[i].dy *= -1.05;
    } else if (balls[i].y > 165) {
      balls[i].y = 165;
      balls[i].dy *= -1.05;
    }

    if (abs(balls[i].dx) > 8.0) balls[i].dx = (balls[i].dx > 0) ? 8.0 : -8.0;
    if (abs(balls[i].dy) > 8.0) balls[i].dy = (balls[i].dy > 0) ? 8.0 : -8.0;

    uint16_t cor = rainbowColor((int)(balls[i].x + balls[i].y) + color_offset);
    canvas.fillCircle(balls[i].x, balls[i].y, 5, cor);
    canvas.fillCircle(balls[i].x, balls[i].y, 2, TFT_WHITE);
  }
  canvas.pushSprite(0, 0);
  color_offset += 3;
}

void init_asteroids() {
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);

  int total_asteroides = constrain(config.qtd_asteroides, 1, MAX_ASTEROIDS);
  for (int i = 0; i < total_asteroides; i++) {
    asteroids[i].x = random(20, 300);
    asteroids[i].y = random(20, 150);
    asteroids[i].vx = random(-3, 4);
    asteroids[i].vy = random(-3, 4);
    if (asteroids[i].vx == 0 && asteroids[i].vy == 0) {
      asteroids[i].vx = 1;
      asteroids[i].vy = 1;
    }
    asteroids[i].size = random(10, 20);
    asteroids[i].points = random(6, 10);
    for (int p = 0; p < asteroids[i].points; p++) {
      asteroids[i].angle[p] = (360 / asteroids[i].points) * p + random(-15, 15);
      asteroids[i].radius[p] = asteroids[i].size + random(-3, 3);
    }
  }
}

void update_asteroids() {
  color_offset += 2;
  if (color_offset > 2000) color_offset = 0;

  int total_asteroides = constrain(config.qtd_asteroides, 1, MAX_ASTEROIDS);
  for (int i = 0; i < total_asteroides; i++) {
    asteroids[i].x += asteroids[i].vx;
    asteroids[i].y += asteroids[i].vy;
    if (asteroids[i].x < 0 || asteroids[i].x > 320) asteroids[i].vx *= -1;
    if (asteroids[i].y < 0 || asteroids[i].y > 172) asteroids[i].vy *= -1;
  }
}

void draw_asteroids() {
  canvas.fillSprite(TFT_BLACK);
  int total_asteroides = constrain(config.qtd_asteroides, 1, MAX_ASTEROIDS);
  for (int i = 0; i < total_asteroides; i++) {
    uint16_t color = rainbowColor(asteroids[i].x + asteroids[i].y);
    for (int p = 0; p < asteroids[i].points; p++) {
      int x1 = asteroids[i].x + cos(radians(asteroids[i].angle[p])) * asteroids[i].radius[p];
      int y1 = asteroids[i].y + sin(radians(asteroids[i].angle[p])) * asteroids[i].radius[p];
      int x2 = asteroids[i].x + cos(radians(asteroids[i].angle[(p + 1) % asteroids[i].points])) * asteroids[i].radius[(p + 1) % asteroids[i].points];
      int y2 = asteroids[i].y + sin(radians(asteroids[i].angle[(p + 1) % asteroids[i].points])) * asteroids[i].radius[(p + 1) % asteroids[i].points];
      canvas.drawLine(x1, y1, x2, y2, color);
    }
  }
  canvas.pushSprite(0, 0);
}

void init_atari() {
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);
  rainbow_offset = 0;
}

uint16_t rainbowColor(int pos) {
  byte r = (sin(0.05 * (pos + color_offset) + 0) * 127) + 128;
  byte g = (sin(0.05 * (pos + color_offset) + 2) * 127) + 128;
  byte b = (sin(0.05 * (pos + color_offset) + 4) * 127) + 128;
  return tft.color565(r, g, b);
}

uint16_t neonPulseColor(int pos, float intensity, int phase) {
  float pulse = (sin(0.02 * color_offset + phase) + 1.0) / 2.0;
  float menu_mult = (float)config.intensidade_cores / 100.0;
  byte r = (sin(0.05 * (pos + color_offset) + 0) * 127 + 128) * pulse * intensity * menu_mult;
  byte g = (sin(0.05 * (pos + color_offset) + 2) * 127 + 128) * pulse * intensity * menu_mult;
  byte b = (sin(0.05 * (pos + color_offset) + 4) * 127 + 128) * pulse * intensity * menu_mult;
  return tft.color565(r, g, b);
}

void update_atari() {
  color_offset += 3;
  if (color_offset > 2000) color_offset = 0;
}

void draw_atari_logo() {
  // O PULO DO GATO DEFINITIVO: Desenhamos os dois logos direto no CANVAS na RAM!
  // Isso otimiza o tráfego SPI em mais de 10x e tira todo o lag do RP2040/RP2350.
  canvas.fillSprite(TFT_BLACK);

  int y0 = 46;
  int x_logo1 = 50;
  int x_logo2 = 196;

  for (int y = 0; y < logoHeight; y++) {
    for (int x = 0; x < logoWidth; x++) {
      uint16_t pixelOriginal = atari[y * logoWidth + x];
      if (pixelOriginal != TFT_BLACK && pixelOriginal != 0x0000) {
        uint16_t newColor = rainbowColor(x + y + color_offset);
        canvas.drawPixel(x_logo1 + x, y0 + y, newColor);
        canvas.drawPixel(x_logo2 + x, y0 + y, newColor);
      }
    }
  }
  canvas.pushSprite(0, 0);  // Joga tudo na tela instantaneamente
}

/* ==================== SCREENSAVER: PAC-MAN ==================== */
void init_pacman() {
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);
  pac_posX = -60;
  prev_pac_posX = -60;
}

void update_pacman() {
  prev_pac_posX = pac_posX;
  pac_posX += 4;
  if (pac_posX > 390) {
    pac_posX = -60;
  }
}

void draw_pacman_ss() {
  // Redesenhando com Canvas para eliminar flickering e lag
  canvas.fillSprite(TFT_BLACK);

  int mouthSize = abs(sin(millis() / 120.0) * 14);
  bool isVulnerable = (pac_posX > 200);

  for (int i = 25; i < 310; i += 24) {
    if (i > pac_posX + 12) {
      canvas.fillCircle(i, 86, (i > 270 ? 5 : 2), TFT_WHITE);
    }
  }

  // Ghost no Canvas
  uint16_t gColor = isVulnerable ? TFT_BLUE : TFT_RED;
  int gx = pac_posX - 45;
  int gy = 86;
  canvas.fillRect(gx - 12, gy - 5, 24, 18, gColor);
  canvas.fillCircle(gx, gy - 5, 12, gColor);
  if (!isVulnerable) {
    canvas.fillCircle(gx - 5, gy - 6, 3, TFT_WHITE);
    canvas.fillCircle(gx + 5, gy - 6, 3, TFT_WHITE);
    canvas.fillCircle(gx - 5, gy - 6, 1, TFT_BLUE);
    canvas.fillCircle(gx + 5, gy - 6, 1, TFT_BLUE);
  } else {
    canvas.fillCircle(gx - 5, gy - 6, 2, TFT_WHITE);
    canvas.fillCircle(gx + 5, gy - 6, 2, TFT_WHITE);
    canvas.drawFastHLine(gx - 6, gy + 5, 12, TFT_WHITE);
  }

  // Pacman no Canvas
  canvas.fillCircle(pac_posX, 86, 15, TFT_YELLOW);
  if (mouthSize > 0) {
    canvas.fillTriangle(pac_posX, 86, pac_posX + 22, 86 - mouthSize, pac_posX + 22, 86 + mouthSize, TFT_BLACK);
  }

  canvas.pushSprite(0, 0);
}

void init_game() {
  tft.fillScreen(TFT_BLACK);
  memset(world_last, 0, sizeof(world_last));

  for (int x = 0; x < GOL_W; x++) {
    for (int y = 0; y < GOL_H; y++) {
      world[x][y] = (random(100) < 35);
    }
  }
  for (int i = 0; i < 4; i++) history_checksum[i] = i;
  stable_count = 0;
}

void update_game() {
  uint32_t pop = 0, cksum = 0;
  for (int x = 0; x < GOL_W; x++) {
    for (int y = 0; y < GOL_H; y++) {
      int n = 0;
      for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
          if (i || j) n += world[(x + i + GOL_W) % GOL_W][(y + j + GOL_H) % GOL_H];
        }
      }
      next_world[x][y] = world[x][y] ? (n == 2 || n == 3) : (n == 3);
      if (next_world[x][y]) {
        pop++;
        cksum += (x * 31 + y * 7);
      }
    }
  }
  if (pop == 0) stable_count = 20;
  memcpy(world, next_world, sizeof(world));
}

void draw_game() {
  canvas.fillSprite(TFT_BLACK);
  for (int x = 0; x < GOL_W; x++) {
    for (int y = 0; y < GOL_H; y++) {
      if (world[x][y]) {
        float valor = (x * 0.04) + (y * 0.06);
        uint8_t r = (sin(valor) * 127) + 128;
        uint8_t g = (sin(valor + 2.0) * 127) + 128;
        uint8_t b = (cos(valor + 1.0) * 127) + 128;
        uint16_t cor_rgb = tft.color565(r, g, b);
        canvas.fillRect(x * 2, y * 2, 2, 2, cor_rgb);
      }
    }
  }
  canvas.pushSprite(0, 0);
}

void init_matrix() {
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);
  for (int i = 0; i < MATRIX_COLS_NEW; i++) {
    matrix_y_new[i] = random(-100, 0);
    matrix_speed_new[i] = random(2, 5);
  }
}

void update_matrix() {
  for (int i = 0; i < MATRIX_COLS_NEW; i++) {
    matrix_y_new[i] += matrix_speed_new[i];
    if (matrix_y_new[i] - 45 > 172) {
      matrix_y_new[i] = random(-45, 0);
      matrix_speed_new[i] = random(2, 5);
    }
  }
}

void draw_matrix() {
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.fillSprite(TFT_BLACK);

  uint8_t brilho_verde = map(config.intensidade_cores, 0, 100, 40, 255);
  uint16_t cor_matrix = tft.color565(0, brilho_verde, 0);

  for (int i = 0; i < MATRIX_COLS_NEW; i++) {
    int current_y = matrix_y_new[i];
    canvas.setTextColor(cor_matrix);
    for (int espaco = 1; espaco < 6; espaco++) {
      int antigo_y = current_y - (espaco * 8);
      if (antigo_y >= 0 && antigo_y < 172) {
        char c_antigo = random(33, 126);
        canvas.drawChar(c_antigo, i * 6, antigo_y, 1);
      }
    }
    if (current_y >= 0 && current_y < 172) {
      char c_cabeca = random(33, 126);
      canvas.setTextColor(TFT_WHITE);
      canvas.drawChar(c_cabeca, i * 6, current_y, 1);
    }
  }
  canvas.pushSprite(0, 0);
}

void init_starfield() {
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);
  for (int i = 0; i < 50; i++) {
    stars[i].x = random(0, 320);
    stars[i].y = random(0, 172);
    stars[i].size = random(1, 3);
    stars[i].speed = (stars[i].size == 1) ? random(1, 3) : random(2, 5);
    int choice = random(0, 3);
    stars[i].color = (choice == 0) ? TFT_WHITE : ((choice == 1) ? tft.color565(255, 255, 128) : tft.color565(128, 200, 255));
  }
}

void update_starfield() {
  for (int i = 0; i < 50; i++) {
    stars[i].y += stars[i].speed;
    if (stars[i].y >= 172) {
      stars[i].y = 0;
      stars[i].x = random(0, 320);
      stars[i].size = random(1, 3);
      stars[i].speed = (stars[i].size == 1) ? random(1, 3) : random(2, 5);
    }
  }
}

void draw_starfield() {
  canvas.fillSprite(TFT_BLACK);
  for (int i = 0; i < 50; i++) {
    canvas.fillRect(stars[i].x, stars[i].y, stars[i].size, stars[i].size, stars[i].color);
  }
  canvas.pushSprite(0, 0);
}

// Função para desenhar o Histograma dos ultimos 7 dias
// Params: x, y (posicao inicial), w, h (largura e altura do grafico)
// Função para desenhar o Histograma dos últimos 7 dias recebendo o array como parâmetro
// Params: dados_dias (ponteiro/array de uint32_t com 7 posições), x, y, w, h
void desenhar_histograma_semanal(const uint32_t dados_dias[7], int x, int y, int w, int h) {
  // 1. Descobrir o valor máximo dos últimos 7 dias para escalar as barras
  uint32_t max_val = 1; // Evita divisão por zero
  for (int i = 0; i < 7; i++) {
    if (dados_dias[i] > max_val) {
      max_val = dados_dias[i];
    }
  }

  // Moldura do gráfico
  canvas.drawRect(x, y, w, h, TFT_WHITE);
  canvas.fillRect(x + 1, y + 1, w - 2, h - 2, tft.color565(15, 15, 15));

  // Parâmetros de layout das 7 barras
  int num_barras = 7;
  int espacamento = 4;
  int largura_barra = (w - 10 - (espacamento * (num_barras - 1))) / num_barras;
  int altura_max_barra = h - 14; // Reserva espaço inferior para os rótulos

  canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
  canvas.setTextDatum(MC_DATUM);

  for (int i = 0; i < 7; i++) {
    // Cálculo proporcional da altura da barra
    int altura_barra = (int)(((float)dados_dias[i] / max_val) * altura_max_barra);
    if (dados_dias[i] > 0 && altura_barra < 2) altura_barra = 2; // Altura mínima visível

    int bx = x + 5 + i * (largura_barra + espacamento);
    int by = y + h - 12 - altura_barra; // Base do gráfico acima do rótulo

    // Destaque de cor: dia atual (posição 6) em ciano
    uint16_t cor_barra = (i == 6) ? TFT_CYAN : TFT_GREEN;

    // Desenha a barra vertical
    if (altura_barra > 0) {
      canvas.fillRect(bx, by, largura_barra, altura_barra, cor_barra);
      canvas.drawRect(bx, by, largura_barra, altura_barra, TFT_WHITE);
    }

    // Rótulo do dia
    char label_dia[4];
    if (i == 6) {
      strcpy(label_dia, "HOJ");
    } else {
      sprintf(label_dia, "D%d", i + 1);
    }

    canvas.setTextColor(TFT_SILVER, tft.color565(15, 15, 15));
    canvas.drawString(label_dia, bx + (largura_barra / 2), y + h - 6);
  }

  canvas.setTextDatum(TL_DATUM); // Restaura alinhamento
}

void renderizar_tela_historico_odometro() {
  canvas.fillScreen(TFT_BLACK);

  // Titulo da Tela
  canvas.setFreeFont(&JetBrainsMono_Bold6pt7b);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.drawString("HISTORICO DE DIGITACAO (7 DIAS)", 10, 10);

  // Exemplo passando a variavel do array no primeiro parametro:
  desenhar_histograma_semanal(config.historico_dias, 10, 30, 300, 125);

  // Envia para o display
  canvas.pushSprite(0, 0);
}

void enviarAntiLock() {
  if (!config.envio_ctrl_shift) return;
  if (usb_hid.ready()) {
    uint8_t mouseMove[5] = { 0x00, 4, 0, 0, 0 };
    usb_hid.sendReport(3, mouseMove, 5);
    delay(40);
    mouseMove[1] = 252;
    usb_hid.sendReport(3, mouseMove, 5);
    delay(40);
  }
}

/* ================================================================
   DRIVERS HID E CALLBACKS NATIVOS DO TINYUSB HOST/DEVICE
   ================================================================ */
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
  if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize > 0 && dev_addr_keyboard != 0) {
    uint8_t led_state = buffer[0];
    if (led_state != g_leds) {
      g_leds = led_state;
      tuh_hid_set_report(dev_addr_keyboard, instance_keyboard, 0, HID_REPORT_TYPE_OUTPUT, (void *)&g_leds, 1);
    }
  }
}

void remap_key(hid_keyboard_report_t const *original, hid_keyboard_report_t *remapped) {
  memcpy(remapped, original, sizeof(hid_keyboard_report_t));
  bool altGr = (original->modifier & KEYBOARD_MODIFIER_RIGHTALT);
  for (uint8_t i = 0; i < 6; i++) {
    uint8_t key = original->keycode[i];
    if (altGr && key == HID_KEY_R) {
      remapped->modifier &= ~KEYBOARD_MODIFIER_RIGHTALT;
      remapped->keycode[i] = 0x64;
    } else if (altGr && key == HID_KEY_M) {
      remapped->modifier &= ~KEYBOARD_MODIFIER_RIGHTALT;
      remapped->modifier |= (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT);
      teams_alert_timeout = millis() + 1500;
      rp2040.fifo.push_nb(FIFO_DISPLAY_UPDATE);
    }
  }
}

void process_kbd_report(hid_keyboard_report_t const *report) {
  static uint64_t last_pressed = 0;
  uint64_t current_pressed = 0;
  for (int i = 0; i < 6; i++)
    if (report->keycode[i]) current_pressed |= (1ULL << report->keycode[i]);
  if (current_pressed & ~last_pressed) g_key_count++;
  last_pressed = current_pressed;
}

extern "C" {
  void tuh_hid_mount_cb(uint8_t d, uint8_t i, uint8_t const *desc, uint16_t l) {
    uint8_t itf_protocol = tuh_hid_interface_protocol(d, i);
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
      dev_addr_keyboard = d;
      instance_keyboard = i;
    }
    tuh_hid_receive_report(d, i);
  }

  void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    if (len == sizeof(hid_keyboard_report_t)) {
      hid_keyboard_report_t const *kbd_report = (hid_keyboard_report_t const *)report;
      process_kbd_report(kbd_report);
      hid_keyboard_report_t remapped;
      remap_key(kbd_report, &remapped);
      if (usb_hid.ready()) usb_hid.sendReport(1, &remapped, sizeof(hid_keyboard_report_t));
    } else if (len == 3 && report[0] == 0x03) {
      int delta = (report[1] == 0xE9) ? 2 : (report[1] == 0xEA) ? -2
                                                                : 0;
      if (delta != 0) {
        g_volume = constrain(g_volume + delta, 0, 100);
        rp2040.fifo.push_nb(FIFO_DISPLAY_UPDATE);
      }
      uint8_t media_data[2] = { report[1], report[2] };
      if (usb_hid.ready()) usb_hid.sendReport(2, media_data, 2);
    }
    tuh_hid_receive_report(dev_addr, instance);
  }
}