# Round 1 — City Street

La demo reconstruye el primer round de *Streets of Rage* para Master System
con gráficos ilustrados, cámara panorámica y efectos modernos, manteniendo la
estructura del nivel original.

## Controles

| Acción | Teclas |
| --- | --- |
| Mover | WASD o flechas |
| Combo | Z, J o Espacio |
| Patada fuerte | X o K |
| Saltar / patada aérea | V o I; atacar durante el salto |
| Ataque especial | C o L |
| Pausa | P |
| Reiniciar | R |
| Salir | Esc |

## Estructura reproducida

- 27 enemigos comunes repartidos en seis paradas.
- Máximo de dos enemigos activos a la vez, como en Master System.
- Secuencia: 3 Galsia + Signal; 2 Signal; 4 Galsia + 2 Signal; 2 Shiva;
  3 Galsia + 2 Electra; 6 Galsia + 2 Electra.
- Jefe final Boomer/Antonio con combate cercano y bumerán de ida y vuelta.
- Seis cabinas rompibles: dos manzanas, botella, tubería, pimienta y carne.
- Temporizador, energía, vidas, puntos, especial, barra del jefe y bonus de
  final de ronda.

La secuencia se contrastó con la guía de Master System de Frank Provo:

https://gamefaqs.gamespot.com/sms/581078-streets-of-rage/faqs/10663

## Compilar

```powershell
.\scripts\build-debug.ps1
```

Ejecutable:

```text
out/cli/x64-debug/Debug/StreetsEnhanced.exe
```

## Accesos de validación

Estas teclas sirven para revisar encuentros concretos durante el desarrollo:

- `F2`: siguiente encuentro.
- `F3`: jefe final.
- `F4`: pareja de Shiva.
- `F5`: pareja de Electra.
- `F6`: primera cabina sin oleadas.
