#include "game.h"
#include "renderEngine.h"
#include <chrono>
#include <thread>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../tiny_obj_loader.h"

typedef std::chrono::duration<float, std::chrono::seconds::period> fSec;
#define AS_SECONDS(x) std::chrono::duration_cast<fSec>(x)
#define NOW std::chrono::high_resolution_clock::now()
#define WAIT(x) std::this_thread::sleep_for(x)

using namespace std::chrono_literals;

int main() {         // Anfang des Programms
  Game game;         // erstellt Game (unterliegende Struktur des Spiels)
  auto lastTp = NOW; // setzt lastTp auf jetzt
  fSec diff =
      AS_SECONDS(1ms); // setzt diff (Zeitdifferenz) auf eine millisekunde
  fSec desiredDiff = AS_SECONDS(17ms); // setzt desiredDiff auf 17 millisekunden
                                       // desiredDiff = gewünschte Zeitdifferenz
  while (game.running()) { // wiederholt eingerücktes bis das Spiel geschlossen
                           // wird
    game.update(diff.count()); // ruft die updatemethode von game mit diff in
                               // sekunden auf

    diff = AS_SECONDS(NOW - lastTp); // berechnet verstrichene Zeit
    if (diff < desiredDiff) {        //
      WAIT(diff - desiredDiff);      // wartet bis desiredDiff verstrichen ist
    } //
    lastTp = NOW; // setzt lastTp auf den jetztigen Zeitpunkt
  }
} // befreit die von Game beanspruchten Ressourcen
