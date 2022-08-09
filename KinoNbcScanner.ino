#include <hiduniversal.h>
#include <hidboot.h>
#include <LiquidCrystal.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(*a))
#define PIEZO_PIN A5

USB Usb;
HIDUniversal BarcodeScanner(&Usb);

// The USB Host Shield is documented to use the D10-D13 SPI pins. And, others
// have found it also requires D7 to be bridged to RST. But even more, trying
// to use D8/D9 for the LCD causes corrupt writes, so it seems to be using them
// as well. Thus, we use D2-D6 and one analog pin.
LiquidCrystal lcd(2, 3, 4, 5, 6, A0);

struct upc_price {
  const char *const upc;
  const unsigned int price_yen;
};

const struct upc_price PRICES[] = {
  { .upc = "4901681143146", .price_yen = 100 }, // Sarasa Clip .7 緑
  { .upc = "4901681143269", .price_yen = 100 }, // Sarasa Clip .7 ベールブルー
  { .upc = "4901681143283", .price_yen = 100 }, // Sarasa Clip .7 スカイブルー
  { .upc = "4901681143290", .price_yen = 100 }, // Sarasa Clip .7 オレンジ
  { .upc = "4901681143498", .price_yen = 100 }, // Sarasa Clip .7 ライトグリーン
  { .upc = "4901681351435", .price_yen = 100 }, // Sarasa Clip .7 レッドオレンジ
  { .upc = "4901681351442", .price_yen = 100 }, // Sarasa Clip .7 ビリジアン
  { .upc = "4901681351459", .price_yen = 100 }, // Sarasa Clip .7 黄
  { .upc = "4901681451425", .price_yen = 100 }, // Sarasa Clip .7 ブルーグリーン
  { .upc = "4902505126215", .price_yen = 200 }, // Hi-Tec-C .3 レッド
  { .upc = "4902505126222", .price_yen = 200 }, // Hi-Tec-C .3 ブルー
  { .upc = "4902505126239", .price_yen = 200 }, // Hi-Tec-C .3 グリーン
  { .upc = "4902505126253", .price_yen = 200 }, // Hi-Tec-C .3 オレンジ
  { .upc = "4902505126260", .price_yen = 200 }, // Hi-Tec-C .3 ピンク
  { .upc = "4902505126277", .price_yen = 200 }, // Hi-Tec-C .3 バイオレット
  { .upc = "4902505126284", .price_yen = 200 }, // Hi-Tec-C .3 ライトブルー
  { .upc = "4902505126291", .price_yen = 200 }, // Hi-Tec-C .3 ブラウン
  { .upc = "4902505162169", .price_yen = 200 }, // Hi-Tec-C .3 ブルーブラック
  { .upc = "4902505162763", .price_yen = 200 }, // Hi-Tec-C .5 黒
  { .upc = "4902506339423", .price_yen = 200 }, // Energel Clena 0.3 サックスブルー
  { .upc = "4902778142554", .price_yen = 150 }, // Signo RT1 .28 黒
  { .upc = "4902778142653", .price_yen = 150 }, // Signo RT1 .38 黒
  { .upc = "4902778164471", .price_yen = 150 }, // Jetstream .38 黒
  { .upc = "4902778579855", .price_yen = 150 }, // Signo 1.0 黒
  { .upc = "4902778588802", .price_yen = 150 }, // Signo 1.0 ホワイト
  { .upc = "4902778745762", .price_yen = 150 }, // Signo DX .28 ブラウンブラック
  { .upc = "4902778805244", .price_yen = 150 }, // Jetstream .7 黒
};

void lcd_print_cents(const int price_cents) {
  lcd.print("$");
  lcd.print(price_cents / 100);
  lcd.print(".");
  unsigned int mod_100 = price_cents % 100;
  if (mod_100 < 10)
    lcd.print("0");
  lcd.print(mod_100);
}

void on_scan(const char *const barcode) {
  static int subtotal_cents = 0;
  lcd.clear();
  for (unsigned int i = 0; i < ARRAY_LEN(PRICES); i++) {
    if (strcmp(barcode, PRICES[i].upc) == 0) {
      lcd.print("OK ");
      lcd.print(barcode);
      lcd.setCursor(0, 1);
      unsigned int price_cents = PRICES[i].price_yen * 2 - 5;
      lcd.print("+");
      lcd_print_cents(price_cents);
      subtotal_cents += price_cents;
      lcd.print(": ");
      lcd_print_cents(subtotal_cents);
      return;
    }
  }
  lcd.print("xx ");
  lcd.print(barcode);
  lcd.setCursor(0, 1);
  lcd.print("(still ");
  lcd_print_cents(subtotal_cents);
  lcd.print(")");
  delay(100);
  tone(PIEZO_PIN, 262, 300);
}

// Note: The HID Boot Keyboard infrastructure does not accept the barcode
// scanner we use. Despite the device having a class/subclass/protocol of
// 3/1/1, it seems the device doesn't present the correct boot-keyboard
// endpoints.  However, it seems we can simply handshake using HIDUniversal
// rather than HIDBoot<USB_HID_PROTOCOL_KEYBOARD>. That being done, we can
// still subclass the HIDBoot's KeyboardReportParser to re-use its parsing
// helpers.
class BarcodeScannerParser : public KeyboardReportParser {
  int curr_idx = 0;
  char curr_barcode[sizeof("4902505162763")];
  void OnKeyDown(uint8_t mod, uint8_t key);
};

void BarcodeScannerParser::OnKeyDown(uint8_t mod, uint8_t key) {
  if (key == UHS_HID_BOOT_KEY_ENTER) {
    curr_barcode[curr_idx] = '\0';
    curr_idx = 0;
    on_scan(curr_barcode);
    return;
  }
  // We trust the scanner to not overflow our buffer.
  curr_barcode[curr_idx++] = OemToAscii(mod, key);
}

BarcodeScannerParser Parser;

void setup() {
  Serial.begin(115200);
  Serial.println("Startup!");
  lcd.begin(16, 2);
  lcd.print("NBC Scanner v1.0");
  if (Usb.Init() == -1) {
    lcd.setCursor(0, 1);
    lcd.print("USB failed...");
    while (true);
  }
  delay(200); // Recommended by USB library.
  BarcodeScanner.SetReportParser(0, &Parser);
}

void loop() {
  Usb.Task();
}
