//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$ 
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 


#include "locomotivebehavior.h"
#include "ctrain_handler.h"

// Définition des contacts pour chaque itinéraire
// Chemin de la locomotive A (sens horaire)
static const std::vector<int> pathA = {34, 1, 5, 7, 9, 11, 19, 21, 23, 25, 27, 29, 31, 33};
// Chemin de la locomotive B (sens anti-horaire)
static const std::vector<int> pathB = {31, 33, 1, 3, 5, 7, 15, 17, 19, 21, 23, 25, 27, 29};
// Section partagée (contacts 5, 7, 19, 21, 23)
static const std::set<int> sharedSection = {5, 7, 19, 21, 23};
// Contacts où les locomotives changent de direction
static const std::set<int> directionChangePoints = {1, 29};

void LocomotiveBehavior::run()
{
    //Initialisation de la locomotive
    loco.allumerPhares();
    loco.demarrer();
    loco.afficherMessage("Ready!");

    /* A vous de jouer ! */

    // Vous pouvez appeler les méthodes de la section partagée comme ceci :
    //sharedSection->access(loco);
    //sharedSection->leave(loco);
    //sharedSection->stopAtStation(loco);

    // Déterminer l'itinéraire en fonction du numéro de la locomotive
    const std::vector<int>& path = (loco.numero() == 7) ? pathA : pathB;
    bool isClockwise = (loco.numero() == 7); // Loco A dans le sens horaire
    
    // Position initiale
    size_t currentIndex = 0;
    int currentContact = path[currentIndex];
    bool inSharedSection = false;
    
    while (true) {
        // On attend qu'une locomotive arrive sur le contact 1.
        // Pertinent de faire ça dans les deux threads? Pas sûr...
        attendre_contact(currentContact);
        loco.afficherMessage(QString("Contact %1").arg(currentContact));
        
        // Vérifier si on entre dans la section partagée
        if (sharedSection.find(currentContact) != sharedSection.end() && !inSharedSection) {
            sharedSection->access(loco, isClockwise ? SharedSectionInterface::Direction::D1 : SharedSectionInterface::Direction::D2);
            inSharedSection = true;
            loco.afficherMessage("Entrée en section partagée");
        }
        // Vérifier si on sort de la section partagée
        else if (inSharedSection && sharedSection.find(currentContact) == sharedSection.end()) {
            sharedSection->leave(loco, isClockwise ? SharedSectionInterface::Direction::D1 : SharedSectionInterface::Direction::D2);
            inSharedSection = false;
            loco.afficherMessage("Sortie de la section partagée");
            
            // Si une autre locomotive attend, on la laisse passer
            sharedSection->release(loco);
        }
        
        // Vérifier si c'est un point de changement de direction
        if (directionChangePoints.find(currentContact) != directionChangePoints.end()) {
            // Changer de direction (inverser le sens de parcours)
            isClockwise = !isClockwise;
            loco.inverserSens();
            loco.afficherMessage(QString("Changement de direction: %1")
                               .arg(isClockwise ? "Horaire" : "Anti-horaire"));
            
            // Ajuster l'index pour le prochain contact
            if (isClockwise) {
                currentIndex = (currentIndex + 1) % path.size();
            } else {
                currentIndex = (currentIndex == 0) ? path.size() - 1 : currentIndex - 1;
            }
        } else {
            // Passer au prochain contact dans la direction actuelle
            if (isClockwise) {
                currentIndex = (currentIndex + 1) % path.size();
            } else {
                currentIndex = (currentIndex == 0) ? path.size() - 1 : currentIndex - 1;
            }
        }
        
        // Mettre à jour le prochain contact à surveiller
        currentContact = path[currentIndex];
    }
}

void LocomotiveBehavior::printStartMessage()
{
    qDebug() << "[START] Thread de la loco" << loco.numero() << "lancé";
    loco.afficherMessage("Je suis lancée !");
}

void LocomotiveBehavior::printCompletionMessage()
{
    qDebug() << "[STOP] Thread de la loco" << loco.numero() << "a terminé correctement";
    loco.afficherMessage("J'ai terminé");
}
