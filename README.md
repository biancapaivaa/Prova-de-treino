# EstufaSmart — Monitoramento inteligente de ambiente

Sistema IoT que acompanha temperatura, umidade, luminosidade e estado da janela. O Arduino avalia as leituras, sinaliza situações normais/de atenção/críticas e envia a telemetria por Wi-Fi para um servidor Flask, que armazena os registros em SQLite e apresenta um painel web.

## 1. Placa utilizada

**Arduino UNO R4 WiFi**

A placa lê os sensores, controla LEDs, buzzer e relé, exibe o estado na matriz de LEDs integrada e envia os dados ao servidor pela rede Wi-Fi.

## 2. Sensores e componentes

| Componente | Pino | Função |
|---|---|---|
| DHT11 | D2 | Mede a temperatura em °C e a umidade relativa do ar em %. |
| Reed switch ou tilt switch (sensor/interruptor da janela) | D3 | Informa se a janela está aberta ou fechada. No código, o contato é lido com `INPUT_PULLUP` e ligado ao GND quando acionado. |
| LDR + resistor de 10 kΩ | A0 | Mede a luminosidade relativa. O programa converte a leitura analógica em uma porcentagem aproximada de 0 a 100%. |
| Botão | D4 | Permite reconhecer localmente o alarme quando o estado está crítico. |
| LED verde | D5 | Indica estado normal. |
| LED amarelo | D6 | Indica atenção ou falha do sensor. |
| LED vermelho | D7 | Indica estado crítico ou alarme reconhecido. |
| Buzzer | D8 | Emite sinal sonoro para alertas e alarmes. |
| Módulo relé | D9 | Aciona uma saída externa, configurada no projeto para representar a ventilação durante estado crítico. Verifique a compatibilidade elétrica antes de ligar qualquer equipamento real. |
| Matriz de LEDs integrada 12×8 | Integrada à placa | Exibe uma letra correspondente ao estado: N (normal), A (atenção), C (crítico), R (reconhecido) ou F (falha do sensor). |

### Ligações principais

- DHT11: DATA → D2.
- Sensor da janela: sinal → D3; contato entre o pino e GND quando acionado.
- Botão: um terminal → D4 e outro → GND.
- LDR: monte um divisor de tensão: **5V → LDR → ponto A0 → resistor de 10 kΩ → GND**.
- LEDs: verde → D5, amarelo → D6, vermelho → D7 (use resistores limitadores adequados).
- Buzzer → D8.
- Entrada de controle do relé (IN) → D9.
- Compartilhe o GND entre os componentes. Confira a tensão e a pinagem dos módulos antes de alimentar o circuito.

## 3. Regras de estado implementadas

- **NORMAL:** temperatura dentro da faixa configurada e sem condições de alerta.
- **ATENCAO:** temperatura próxima/fora da faixa ideal, janela aberta por tempo suficiente ou luminosidade muito alta.
- **CRITICO:** temperatura fora da faixa por tempo prolongado ou janela aberta por tempo prolongado.
- **RECONHECIDO:** alarme crítico reconhecido pelo botão; a condição precisa normalizar para retornar ao estado normal.
- **FALHA_SENSOR:** três falhas consecutivas de leitura do DHT11.

Parâmetros atualmente definidos no código:
- Faixa de temperatura ideal: 20–28 °C.
- Atenção de temperatura: ≤ 21 °C ou ≥ 27 °C.
- Luminosidade muito alta: ≥ 85%.
- Leitura e envio: a cada 5 segundos.
- Atenção por janela aberta: após 20 segundos.
- Estado crítico por janela aberta: após 60 segundos.
- Estado crítico por temperatura fora da faixa: após 30 segundos.

A porcentagem do LDR é uma escala relativa baseada na leitura analógica, não uma medida calibrada em lux.

## 4. Estrutura do projeto

```text
MODULO_B/
├── README.md
├── arduino/
│   └── EstufaSmart/
│       ├── EstufaSmart.ino
│       └── secrets.h
└── servidor/
    ├── app.py
    └── requirements.txt
```

## 5. Como instalar e executar o servidor

O servidor utiliza **Python, Flask e SQLite**. O banco `estufasmart.db` é criado automaticamente quando o servidor é iniciado pela primeira vez.

### Requisitos

- Python 3 instalado no computador.
- Arduino e computador conectados à mesma rede Wi-Fi, para comunicação local.
- Acesso ao terminal (PowerShell no Windows).

### Windows — PowerShell

1. Abra o PowerShell e entre na pasta `servidor` do projeto. Ajuste o caminho conforme o local em que você salvou a pasta:

   ```powershell
   cd caminho\para\MODULO_B\servidor
   ```

2. Crie o ambiente virtual:

   ```powershell
   py -m venv .venv
   ```

3. Ative o ambiente:

   ```powershell
   .\.venv\Scripts\Activate.ps1
   ```

   Se o PowerShell bloquear a ativação, você pode executar os comandos usando o Python do ambiente virtual diretamente, sem ativá-lo:

   ```powershell
   .\.venv\Scripts\python.exe -m pip install -r requirements.txt
   .\.venv\Scripts\python.exe app.py
   ```

4. Instale as dependências:

   ```powershell
   python -m pip install -r requirements.txt
   ```

5. Inicie o servidor:

   ```powershell
   python app.py
   ```

6. No computador, abra **http://localhost:5000** para acessar o painel.

Mantenha o terminal aberto enquanto estiver usando o sistema. Para parar o servidor, pressione `Ctrl+C`.

### Configurar o Arduino para enviar dados

1. Abra `arduino/EstufaSmart/EstufaSmart.ino` na Arduino IDE.
2. Instale a biblioteca **DHT sensor library** (Adafruit) e sua dependência **Adafruit Unified Sensor**. `WiFiS3` e `Arduino_LED_Matrix` acompanham o suporte de placas do Arduino UNO R4 WiFi.
3. Abra `secrets.h` e preencha:
   - `WIFI_SSID`: nome da rede Wi-Fi;
   - `WIFI_PASS`: senha da rede;
   - `SERVER_HOST`: endereço IPv4 do computador que executa o servidor;
   - `SERVER_PORT`: `5000`.
4. Para descobrir o IPv4 no Windows, execute em outro terminal:

   ```powershell
   ipconfig
   ```

   Use o IPv4 do adaptador conectado à mesma rede Wi-Fi do Arduino. **Não use `localhost` ou `127.0.0.1` em `SERVER_HOST`**, pois esses endereços apontam para a própria placa, não para o computador.
5. Selecione **Arduino UNO R4 WiFi** na Arduino IDE, escolha a porta correta e carregue o programa.
6. Abra o Monitor Serial em **115200 baud** para acompanhar as leituras e o estado da conexão.

Se outros dispositivos da rede não conseguirem acessar o servidor, confira as regras do Firewall do Windows para permitir conexões na porta TCP 5000. O servidor é iniciado em `0.0.0.0:5000` para aceitar conexões pela rede local.

## 6. Endpoints da API

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/` | Exibe o painel web. |
| `POST` | `/api/telemetria` | Recebe uma leitura em JSON e grava no SQLite. |
| `GET` | `/api/dados?limit=60` | Retorna as leituras recentes (limite máximo: 500). |
| `GET` | `/api/ultima` | Retorna a leitura mais recente. |

O Arduino envia os campos `device_id`, `placa`, `seq`, `temp`, `umid`, `janela`, `luz_pct` e `estado`. O painel consulta os dados periodicamente e atualiza os indicadores e o gráfico de temperatura.

## 7. Resolução rápida de problemas

- **O painel abre, mas não há dados:** confira se o Arduino está conectado ao Wi-Fi, se `SERVER_HOST` contém o IPv4 correto e se o Firewall permite a porta 5000.
- **Erro ao instalar Flask:** confirme que o ambiente virtual está ativo e execute `python -m pip install -r requirements.txt`.
- **Falha no DHT11:** verifique alimentação, GND, pino DATA em D2 e a biblioteca instalada.
- **Leitura de luz estranha:** confira a montagem do divisor LDR/resistor e lembre que a porcentagem não representa lux calibrado.
- **Relé funciona invertido:** o código possui `RELE_ATIVO_LOW`; ajuste esse parâmetro conforme o módulo utilizado.
- **O banco não aparece:** ele é criado na pasta `servidor` ao iniciar `app.py`; confira se o terminal está executando o arquivo dessa pasta.

## 8. Segurança e configuração

O arquivo `secrets.h` contém credenciais de Wi-Fi. Não publique a senha da rede em repositórios públicos; mantenha credenciais reais fora de commits e compartilhe apenas uma versão de exemplo sem dados pessoais.
