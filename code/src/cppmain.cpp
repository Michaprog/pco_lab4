//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$ 
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 
                                                                            
#include "ctrain_handler.h"

#include "locomotive.h"
#include "locomotivebehavior.h"
#include "sharedsectioninterface.h"
#include "sharedsection.h"

#include <memory>
#include <thread>

// Variables globales pour la section partagée et les threads
static std::shared_ptr<SharedSection> sharedSection;
static std::unique_ptr<LocomotiveBehavior> locoBehavA;
static std::unique_ptr<LocomotiveBehavior> locoBehavB;
static std::thread threadA;
static std::thread threadB;

// Locomotives :
// Vous pouvez changer les vitesses initiales, ou utiliser la fonction loco.fixerVitesse(vitesse);
// Laissez les numéros des locos à 0 et 1 pour ce laboratoire

// Locomotive A
static Locomotive locoA(7 /* Numéro (pour commande trains sur maquette réelle) */, 10 /* Vitesse */);
// Locomotive B
static Locomotive locoB(42 /* Numéro (pour commande trains sur maquette réelle) */, 12 /* Vitesse */);

//Arret d'urgence
void emergency_stop()
{
    // Arrêter toutes les locomotives
    locoA.arreter();
    locoB.arreter();
    
    // Libérer la section partagée
    if (sharedSection) {
        sharedSection->stopAll();
    }
    
    // Afficher un message d'arrêt
    afficher_message("\nARRÊT D'URGENCE !");
}

// Fonction pour initialiser les aiguillages
void initializeSwitches() {
    // Configuration des aiguillages pour la maquette A
    // Ajustez ces valeurs selon votre configuration de maquette
    diriger_aiguillage(1,  TOUT_DROIT, 0);
    diriger_aiguillage(2,  DEVIE     , 0);
    diriger_aiguillage(3,  DEVIE     , 0);
    diriger_aiguillage(4,  TOUT_DROIT, 0);
    diriger_aiguillage(5,  TOUT_DROIT, 0);
    diriger_aiguillage(6,  TOUT_DROIT, 0);
    diriger_aiguillage(7,  TOUT_DROIT, 0);
    diriger_aiguillage(8,  DEVIE     , 0);
    diriger_aiguillage(9,  DEVIE     , 0);
    diriger_aiguillage(10, TOUT_DROIT, 0);
    diriger_aiguillage(11, TOUT_DROIT, 0);
    diriger_aiguillage(12, TOUT_DROIT, 0);
    diriger_aiguillage(13, TOUT_DROIT, 0);
    diriger_aiguillage(14, DEVIE     , 0);
    diriger_aiguillage(15, DEVIE     , 0);
    diriger_aiguillage(16, TOUT_DROIT, 0);
    diriger_aiguillage(17, TOUT_DROIT, 0);
    diriger_aiguillage(18, TOUT_DROIT, 0);
    diriger_aiguillage(19, TOUT_DROIT, 0);
    diriger_aiguillage(20, DEVIE     , 0);
    diriger_aiguillage(21, DEVIE     , 0);
    diriger_aiguillage(22, TOUT_DROIT, 0);
    diriger_aiguillage(23, TOUT_DROIT, 0);
    diriger_aiguillage(24, TOUT_DROIT, 0);
    // diriger_aiguillage(/*NUMERO*/, /*TOUT_DROIT | DEVIE*/, /*0*/);
}

// Fonction principale
int cmain()
{
    /************
     * Maquette *
     ************/

    // Choix de la maquette (A ou B)
    selection_maquette(MAQUETTE_A);

    /**********************************
     * Initialisation des aiguillages *
     **********************************/
    initializeSwitches();

    /********************************
     * Position de départ des locos *
     ********************************/

    // Loco 0
    // Exemple de position de départ
    locoA.fixerPosition(34, 5);

    // Loco 1
    // Exemple de position de départ
    locoB.fixerPosition(31, 1);

    /***********
     * Message *
     **********/

    // Affiche un message dans la console de l'application graphique
    afficher_message("Hit play to start the simulation...");

    /*********************
     * Section partagée  *
     ********************/

    // Création de la section partagée
    sharedSection = std::make_shared<SharedSection>();

    /*******************
     * Threads des locos *
     *******************/

    // Création des comportements des locomotives
    locoBehavA = std::make_unique<LocomotiveBehavior>(locoA, sharedSection);
    locoBehavB = std::make_unique<LocomotiveBehavior>(locoB, sharedSection);

    // Démarrage des threads
    locoBehavA->startThread();
    locoBehavB->startThread();

    /******************
     * Attente fin    *
     *****************/

    // Attente de la fin des threads (ne devrait jamais arriver)
    locoBehavA->join();
    locoBehavB->join();

    //Fin de la simulation
    mettre_maquette_hors_service();

    return EXIT_SUCCESS;
}
