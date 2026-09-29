# RFID MQTT com ESP32

Projeto de leitor RFID com ESP32 e módulo MFRC522, usando comunicação MQTT para autenticação e gerenciamento de cartões.

## Objetivo

- Ler UID de tags RFID
- Validar acesso por cartão cadastrado
- Registrar cadastro e remoção de cartões via tópicos MQTT
- Publicar eventos de acesso e cadastro em um broker

## Hardware

- ESP32
- MFRC522
- LED indicador
- Conexão Wi-Fi

## Simulação online

Projeto no Wokwi:
https://wokwi.com/projects/476541020468548609

## Como usar

1. Abra o projeto no Arduino IDE ou em um ambiente compatível com ESP32.
2. Carregue o código presente em `sketch.ino` para o microcontrolador.
3. Ajuste as configurações de rede e MQTT, caso necessário.
4. Monitore os tópicos MQTT para cadastro, acesso e lista de cartões.

## Estrutura

- `sketch.ino` — código principal do firmware
- `diagram.json` — diagrama do projeto
- `libraries.txt` — bibliotecas necessárias
- `pic/` — imagens e assets do projeto

## Observação

Este projeto foi desenvolvido como exemplo de IoT com RFID e MQTT, sendo ideal para estudos, prototipagem e simulação em Wokwi.
