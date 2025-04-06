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
            {"Connection failed", "Echec de la connexion"},
            {"No network found", "Reseau introuvable"},
            {"Authentication failed", "Echec de l'authentification"},

            // For alarms
            {"Next: ", "Prochain: "},
            {"Al 1: ", "Al 1: "},
            {"Al 2: ", "Al 2: "},
            {"Loud", "Fort"},
            {"Gradual", "Progressif"},
            {"Skip next alarm: ", "Ignorer prochain: "},
            {"Once", "Une fois"},
            {"Weekly", "Hebdo"},

            // For options
            {"Auto light: ", "Luminosite auto: "},
            {"Time format: ", "Format heure: "},
            {"24h", "24h"},
            {"12h", "12h"},
            {"Date format: ", "Format date: "},
            {"Hourly chime: ", "Bip horaire: "},
            {"Auto scroll: ", "Defilement auto: "},
            {"Brightness: ", "Luminosite: "},
            {"Dark brightness: ", "Luminosite sombre: "},
            {"Dim brightness: ", "Luminosite faible: "},
            {"Max brightness: ", "Luminosite max: "},
            {"On", "Oui"},
            {"Off", "Non"},
            {"Day", "Jour"},

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
            {"Daily sync time: ", "Synchro quotidienne: "},
            {"Last sync: ", "Derniere synchro: "},
            {"Last drift: ", "Dernier decalage: "},
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