#include "UiTexts.h"

namespace
{
    const char *g_textTable[static_cast<unsigned int>(TextId::TextCount)][LanguageCount] =
        {
            // Main menu
            {"Alarms", "Alarmes"},
            {"Tools", "Outils"},
            {"Clock sync", "Synchro"},
            {"Options", "Options"},
            {"Exit", "Sortir"},

            // Wifi status
            {"Unknown", "Inconnu"},
            {"OK", "OK"},
            {"Not available", "Non disponible"},
            {"Down", "Hors service"},
            {"Connecting", "Connexion"},
            {"No IP", "Pas d'IP"},
            {"Connected", "Connecte"},
            {"Connection failed", "Echec de connexion"},
            {"No network found", "Reseau introuvable"},
            {"Authentication failed", "Echec de l'authentification"},

            // For alarms
            {"Next: ", "Prochain: "},
            {"Al 1: ", "Al 1: "},
            {"Al 2: ", "Al 2: "},
            {"Loud beeps", "Bips forts"},
            {"Gradual beeps", "Bips progressifs"},
            {"Music", "Musique"},
            {"Volume: ", "Volume: "},
            {"Track: ", "Piste: "},
            {"Random", "Aleatoire"},
            {"Skip next alarm: ", "Ignorer prochain: "},
            {"Once", "Une fois"},
            {"Weekly", "Hebdo"},

            // For options
            {"Auto light: ", "Luminosite auto: "},
            {"Time format: ", "Heure: Format: "},
            {"24h", "24h"},
            {"12h", "12h"},
            {"Date format: ", "Date: Format: "},
            {"Hourly chime: ", "Carillon horaire: "},
            {"Auto scroll: ", "Defilement auto: "},
            {"Brightness: ", "Reglage Luminosite: "},
            {"Dark brightness: ", "Env noir: Luminosite: "},
            {"Dim brightness: ", "Env sombre: Luminosite: "},
            {"Max brightness: ", "Env clair: Luminosite: "},
            {"On", "Oui"},
            {"Off", "Non"},
            {"Beep", "Bip"},
            {"If day: Beep", "Si jour: Bip"},
            {"Sound effect" , "Effet sonore"},
            {"If day: sound effect", "Si jour: effet sonore"},
            {"Chime volume: ", "Volume carillon: "},

            // Tools menu, stopwatch and countdown
            {"Flashlight", "Lampe"},
            {"Stopwatch", "Chrono"},
            {"Reset", "Remettre a zero"},
            {"Countdown", "Minuterie"},
            {"Set", "Regler"},

            // Date formats
            {"MM-DD", "MM-JJ"},
            {"MM/DD", "MM/JJ"},
            {"DD-MM", "JJ-MM"},
            {"DD/MM", "JJ/MM"},
            {"DD.MM", "JJ.MM"},

            // Sync menu
            {"Source: ", "Source: "},
            {"Sync now", "Synchroniser"},
            {"Daily sync time: ", "Heure synchro quotidienne: "},
            {"Last sync: ", "Derniere synchro: "},
            {"Last drift: ", "Decalage derniere synchro: "},
            {"Wifi: ", "Wifi: "},

            // Sync source
            {"RTC", "RTC"},
            {"NTP", "NTP"},
            {"GPS", "GPS"},
    };
}

std::string uiText(TextId id)
{
    return g_textTable[static_cast<unsigned int>(id)][LANGUAGE];
}