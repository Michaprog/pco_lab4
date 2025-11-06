//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$ 
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 

//  Interface de la section partagée.      //
//  Rien à modifier ici.                   //
//                                         //

#ifndef SHAREDSECTIONINTERFACE_H
#define SHAREDSECTIONINTERFACE_H

/**
 * @brief Forward declaration de la classe Locomotive
 * (permet de déclarer des pointeurs/références vers Locomotive
 * sans inclure le header complet)
 */
class Locomotive;

/**
 * @brief Interface définissant les méthodes que toute section partagée
 * doit implémenter pour gérer la synchronisation des locomotives.
 */
class SharedSectionInterface
{
public:
    /**
     * @brief Direction représente le sens de circulation d’une locomotive
     * dans la section partagée.
     */
    enum class Direction { D1, D2 };

    /**
     * @brief Méthode appelée lorsqu’une locomotive souhaite accéder
     * à la section partagée. Bloque si la section n’est pas libre.
     *
     * @param loco      Locomotive demandant l’accès
     * @param d         Direction de déplacement de la locomotive
     */
    virtual void access(Locomotive& loco, Direction d) = 0;

    /**
     * @brief Méthode appelée lorsque la locomotive a quitté physiquement
     * la section
     *
     * @param loco      Locomotive quittant la section
     * @param d         Direction de déplacement de la locomotive
     */
    virtual void leave(Locomotive& loco, Direction d) = 0;

    /**
     * @brief Méthode appelée pour libérer la section après un `leave()`,
     * autorisant éventuellement une autre locomotive à entrer.
     *
     * @param loco      Locomotive qui libère la section
     */
    virtual void release(Locomotive& loco) = 0;

    /**
     * @brief Stoppe toutes les locomotives qui attendent d’accéder
     * à la section partagée (utilisé pour un arrêt d’urgence).
     */
    virtual void stopAll() = 0;

    /**
     * @brief Retourne le nombre d’erreurs détectées dans le protocole
     * (incohérences d’accès, erreurs de séquence, etc.)
     *
     * @return nombre total d’erreurs enregistrées
     */
    [[nodiscard]]
    virtual int nbErrors() = 0;
};

#endif // SHAREDSECTIONINTERFACE_H
