#define PIN_LED 7

int count = 1;
int toggle = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(PIN_LED, toggle);
  delay(1000);
  
  while(1) {
    toggle = 1 - toggle; //toggle = !toggle;
    if (count <= 10) {
      digitalWrite(PIN_LED, toggle);
      Serial.println(count);
      count++;
      delay(100);
      
    } else {
      digitalWrite(PIN_LED, 1);
    }
  }
}