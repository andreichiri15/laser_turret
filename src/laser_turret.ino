#include <LiquidCrystal_I2C.h>

#include <Wire.h>

#include <Servo.h>

#define downServoPin 9
#define upServoPin 10

#define myBaudrate 9600

Servo my_servo;
Servo servo_sus;
LiquidCrystal_I2C lcd(0x27, 16, 2);

char rx_buffer[32];

String serialData;

volatile unsigned long timerMs = 0;
volatile bool laserOn = false;
volatile bool cooldownActive = false;

const unsigned long laserDuration = 200;
const unsigned long laserCooldown = 5000;

unsigned long laserStartTime = 0;
unsigned long cooldownStartTime = 0;

bool last_clicked = false;

bool shooting = false;

void setup_timer() {
  cli();

  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2  = 0;

  OCR2A = 249;
  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS22);
  TIMSK2 |= (1 << OCIE2A);

  sei();
}

void usart_init(unsigned int baud) {
  unsigned int ubrr = (F_CPU / 16 / baud) - 1;
  UBRR0H = (ubrr >> 8);
  UBRR0L = ubrr;

  UCSR0B = (1 << RXEN0) | (1 << TXEN0);     // Enable RX and TX
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   // 8-bit data
  // UCSR0C |= (3<<UCSZ00);
  // UCSR0C &= ~(1 << USBS0);
  // UCSR0C |= 1<<UPM01;
}

void usart_send_char(char c) {
  while (!(UCSR0A & (1 << UDRE0))); // Wait until transmit buffer is empty

  UDR0 = c;
}

void usart_send_string(const char* str) {
  while (*str) {
    usart_send_char(*str);

    str++;
  }
}

char usart_receive_char() {
  while (!(UCSR0A & (1 << RXC0)));
  return UDR0;
}

void usart_read_line(char* buffer, byte maxLen) {
  byte i = 0;
  char c;

  while (i < maxLen - 1) {
    c = usart_receive_char();
    if (c == '\n' || c == '\r')
      break;
    buffer[i] = c;

    i++;
  }

  buffer[i] = '\0';
}

void setup() {
  // put your setup code here, to run once:
  my_servo.attach(downServoPin);
  servo_sus.attach(upServoPin);
  // pinMode(2, OUTPUT);
  DDRD |= (1 << PD2);

  // Serial.begin(9600);
  // Serial.setTimeout(50);

  lcd.init();
  lcd.backlight();

  setup_timer();

  usart_init(myBaudrate);
}

int parseDataX(String data) {
  data.remove(data.indexOf("Y"));
  data.remove(data.indexOf("X"), 1);

  return data.toInt();
}

int parseDataY(String data) {
  data.remove(0, data.indexOf("Y") + 1);

  return data.toInt();
}

ISR(TIMER2_COMPA_vect) {
  timerMs++;
}

void loop() {
  // String serialData = Serial.readStringUntil('\n');
  // String serialData = "X123Y124";

  usart_read_line(rx_buffer, sizeof(rx_buffer));
  String serialData = String(rx_buffer);

  if (serialData.length() < 3)
    return;

  usart_send_string("RAW: ");
  usart_send_string(serialData.c_str());
  usart_send_char('\n');

  bool clickDetected = serialData.indexOf("C") != -1;

  if (clickDetected) {
    serialData.remove(serialData.indexOf("C"), 1);
  }

  int x_value_read = parseDataX(serialData);

  int y_value_read = parseDataY(serialData);

  usart_send_string("X=");
  usart_send_char('0' + (x_value_read / 100) % 10);
  usart_send_char('0' + (x_value_read / 10) % 10);
  usart_send_char('0' + (x_value_read % 10));

  usart_send_string(" Y=");
  usart_send_char('0' + (y_value_read / 100) % 10);
  usart_send_char('0' + (y_value_read / 10) % 10);
  usart_send_char('0' + (y_value_read % 10));
  usart_send_char('\n');

  int angle_x = map(x_value_read, 0, 800, 180, 0);

  int angle_y = map(y_value_read, 0, 600, 60, 180);

  my_servo.write(angle_x);

  // my_servo.write(0);

  servo_sus.write(angle_y);

  if (!clickDetected && last_clicked && timerMs > 5000) {
    timerMs = 0;
  }

  int time_remaining = laserCooldown - timerMs;

  if (clickDetected && time_remaining <= 0) {
    PORTD |= (1 << PD2);   // Set pin 2 HIGH
  } else {
    PORTD &= ~(1 << PD2);  // Set pin 2 LOW
  }

  lcd.setCursor(0, 0);

  lcd.print("Cooldown");

  lcd.setCursor(0, 1);

  // time_remaining = max(time_remaining, -1);
  timerMs = min(timerMs, 6000);

  if (time_remaining > 0) {
    lcd.print(ceil(time_remaining / 1000));

    lcd.setCursor(1, 1);

    lcd.print("      ");
  } else {
    lcd.print("READY");
  }

  last_clicked = clickDetected;
}
