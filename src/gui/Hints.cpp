#include "Hints.h"
#include <map>

namespace pg::ui
{
namespace
{
struct Text
{
    const char* en;
    const char* ru;
};

const std::map<juce::String, Text>& table()
{
    static const std::map<juce::String, Text> t = {
        // ---- Modules
        { "eq", { "Removes rumble, fizz and resonances. Each band works on Stereo, Mid or Side.",
                  "Убирает гул, «песок» и резонансы. Каждая полоса работает на Stereo, Mid или Side." } },
        { "comp", { "Compresses only the low band below SPLIT. The pick attack stays open.",
                    "Сжимает только низкую полосу ниже SPLIT. Атака медиатора остаётся открытой." } },
        { "wide", { "Makes the double-track wider or narrower with Mid/Side. No delay, mono stays safe.",
                    "Делает даблтрек шире или уже через Mid/Side. Без задержки, моно остаётся безопасным." } },
        { "mono", { "Makes the low end mono below FREQ. No phase cancellation.",
                    "Делает низ моно ниже FREQ. Без фазового подавления." } },
        { "pan", { "Moves both guitars to the left or right. No guitar becomes silent.",
                   "Сдвигает обе гитары влево или вправо. Ни одна гитара не пропадает." } },
        { "lofi", { "Lo-fi sound: narrow band, bits, noise, wobble. Automate the LOFI switch.",
                    "Лоу-фай звук: узкая полоса, биты, шум, плавание тона. Автоматизируйте переключатель LOFI." } },
        { "lim", { "Soft clip and true-peak limiter. Controls the pick transients and sets the maximum level.",
                   "Мягкий клиппер и true-peak лимитер. Контролирует атаку медиатора и задаёт максимальный уровень." } },
        { "ag", { "Sets the output loudness equal to the input loudness. Use it for a fair A/B comparison.",
                  "Выравнивает громкость выхода по громкости входа. Используйте для честного сравнения A/B." } },

        // ---- Bus EQ
        { "eq_b_freq", { "Frequency of the selected band.", "Частота выбранной полосы." } },
        { "eq_b_gain", { "Boost or cut of the selected band.", "Подъём или срез выбранной полосы." } },
        { "eq_b_q", { "Width of the band. High Q = narrow. Use Q 10–20 to remove a resonance.",
                      "Ширина полосы. Большой Q = узко. Q 10–20 убирает резонанс." } },
        { "eq_hp", { "Removes the frequencies below this value.", "Убирает частоты ниже этого значения." } },
        { "eq_lp", { "Removes the frequencies above this value.", "Убирает частоты выше этого значения." } },
        { "eq_out", { "Output level of the EQ.", "Выходной уровень эквалайзера." } },
        { "band", { "Selects the EQ band for the FREQ, GAIN and Q knobs.", "Выбирает полосу эквалайзера для ручек FREQ, GAIN и Q." } },
        { "tgt0", { "The band works on the left and right channels.", "Полоса работает на левом и правом каналах." } },
        { "tgt1", { "The band works on the center (both guitars together).", "Полоса работает на центре (обе гитары вместе)." } },
        { "tgt2", { "The band works on the stereo difference (the width).", "Полоса работает на стерео-разнице (ширине)." } },
        { "type0", { "Boost or cut near the frequency.", "Подъём или срез около частоты." } },
        { "type1", { "Full cut at the frequency. GAIN has no effect.", "Полный срез на частоте. GAIN не работает." } },
        { "type2", { "Boost or cut below the frequency.", "Подъём или срез ниже частоты." } },
        { "type3", { "Boost or cut above the frequency.", "Подъём или срез выше частоты." } },
        { "slope0", { "High-pass filter slope 12 dB per octave. Soft.", "Крутизна фильтра высоких частот 12 дБ на октаву. Мягко." } },
        { "slope1", { "High-pass filter slope 24 dB per octave. Steep.", "Крутизна фильтра высоких частот 24 дБ на октаву. Круто." } },
        { "eqgraph", { "Drag a node: frequency and gain. Mouse wheel: Q. Double-click: gain 0 dB.",
                       "Тяните точку: частота и усиление. Колесо мыши: Q. Двойной клик: усиление 0 дБ." } },

        // ---- Bus Compressor
        { "comp_split", { "Crossover frequency. The compressor works below it.", "Частота разделения. Компрессор работает ниже неё." } },
        { "comp_thresh", { "The compressor starts above this level.", "Компрессор начинает работать выше этого уровня." } },
        { "comp_ratio", { "Amount of compression. 1:1 = none.", "Степень сжатия. 1:1 = нет сжатия." } },
        { "comp_attack", { "Time to start the gain reduction.", "Время начала снижения усиления." } },
        { "comp_release", { "Time to stop the gain reduction.", "Время прекращения снижения усиления." } },
        { "comp_sc", { "The detector ignores the frequencies below this value.", "Детектор не реагирует на частоты ниже этого значения." } },
        { "comp_link", { "100% = same gain reduction in left and right.", "100% = одинаковое снижение усиления слева и справа." } },
        { "comp_makeup", { "Adds gain to the low band.", "Добавляет усиление низкой полосе." } },
        { "compgr", { "Gain reduction of the low band.", "Снижение усиления низкой полосы." } },

        // ---- Wider
        { "wide_width", { "Stereo width. 0% = mono, 100% = no change.", "Стерео-ширина. 0% = моно, 100% = без изменений." } },
        { "wide_air", { "Adds width to the high frequencies.", "Добавляет ширину на высоких частотах." } },
        { "wide_cut", { "Removes stereo below this frequency.", "Убирает стерео ниже этой частоты." } },
        { "corr", { "Stereo correlation. Keep the indicator above the center line.", "Стерео-корреляция. Держите индикатор выше центральной линии." } },

        // ---- Mono-Maker
        { "mono_freq", { "Below this frequency the sound becomes mono.", "Ниже этой частоты звук становится моно." } },
        { "mono_width", { "Width below FREQ. 0% = full mono.", "Ширина ниже FREQ. 0% = полное моно." } },

        // ---- True Stereo Pan
        { "pan_pan", { "Moves both guitars to the left or right.", "Сдвигает обе гитары влево или вправо." } },
        { "pan_spread", { "Distance between the guitars. 0% = mono.", "Расстояние между гитарами. 0% = моно." } },

        // ---- Lofi
        { "lofisw", { "Turns the lo-fi sound on or off. Automate it in the song.", "Включает или выключает лоу-фай звук. Автоматизируйте его в песне." } },
        { "lofi_mix", { "Mix of lo-fi and original sound.", "Смесь лоу-фай и исходного звука." } },
        { "lofi_tone", { "Center of the band. Low = dark \"telephone\" sound.", "Центр полосы. Мало = тёмный «телефонный» звук." } },
        { "lofi_bits", { "Bit depth. Fewer bits = more digital noise.", "Разрядность. Меньше бит = больше цифрового шума." } },
        { "lofi_rate", { "Sample rate. Low = metallic aliasing.", "Частота дискретизации. Мало = металлический алиасинг." } },
        { "lofi_noise", { "Hiss and vinyl crackle.", "Шипение и треск винила." } },
        { "lofi_wobble", { "Slow pitch drift (tape wobble).", "Медленное плавание тона (детонация плёнки)." } },
        { "lofi_drive", { "Soft saturation. The level is compensated.", "Мягкая сатурация. Уровень компенсируется." } },

        // ---- Bus Limiter
        { "lim_gain", { "Input gain. More gain = louder and more limiting.", "Входное усиление. Больше = громче и сильнее лимитирование." } },
        { "lim_clip", { "Rounds the peaks before the limiter.", "Скругляет пики перед лимитером." } },
        { "lim_ceiling", { "Maximum output level (true peak).", "Максимальный выходной уровень (true peak)." } },
        { "lim_release", { "Base release time.", "Базовое время восстановления." } },
        { "lim_adapt", { "Slower release on long gain reduction. Prevents low-end distortion.",
                         "Медленнее восстановление при долгом сжатии. Предотвращает искажения низа." } },
        { "lim_out", { "Output level after the limiter.", "Выходной уровень после лимитера." } },
        { "look0", { "No latency. For live play.", "Без задержки. Для игры вживую." } },
        { "look1", { "Exact true peak. Adds 1.5 ms of latency.", "Точный true peak. Добавляет задержку 1.5 мс." } },
        { "look2", { "Exact true peak, the smoothest. Adds 5 ms of latency. For mixing.", "Точный true peak, самый плавный. Задержка 5 мс. Для сведения." } },
        { "os0", { "4x oversampling. Low CPU load.", "Оверсэмплинг 4×. Малая нагрузка на процессор." } },
        { "os1", { "8x oversampling. More accurate peaks.", "Оверсэмплинг 8×. Более точные пики." } },
        { "os2", { "16x oversampling. The most accurate. The highest CPU load.", "Оверсэмплинг 16×. Самый точный. Самая большая нагрузка на процессор." } },
        { "limscope", { "Gray: input. Pink: output. Dark line: gain reduction. Dashed lines: ceiling.",
                        "Серое: вход. Розовое: выход. Тёмная линия: снижение усиления. Пунктир: потолок." } },
        { "limmeters", { "Input level, gain reduction, output level.", "Входной уровень, снижение усиления, выходной уровень." } },

        // ---- Auto-Gain
        { "ag_speed", { "Time to follow a loudness change.", "Время, за которое усиление догоняет изменение громкости." } },
        { "ag_range", { "Maximum gain correction (up and down).", "Максимальная коррекция усиления (вверх и вниз)." } },
        { "ag_trim", { "Offset after the matching. 0 dB = equal loudness.", "Сдвиг после выравнивания. 0 дБ = равная громкость." } },
        { "lufs", { "Measures the loudness as the ear hears it (K-weighting). Best for matching by ear.",
                    "Измеряет громкость так, как слышит ухо (K-взвешивание). Лучше всего для выравнивания на слух." } },
        { "rms", { "Measures the plain signal energy. The low end has more weight.", "Измеряет чистую энергию сигнала. Низ весит больше." } },
        { "hold", { "Freezes the current gain. Then you hear the real level change of the knobs.",
                    "Замораживает текущее усиление. Тогда слышно настоящее изменение уровня от ручек." } },
        { "agmeter", { "Gain that Auto-Gain applies now. Up = boost, down = cut.", "Усиление, которое Auto-Gain применяет сейчас. Вверх = подъём, вниз = срез." } },

        // ---- Common and header
        { "power", { "Turns the module on or off.", "Включает или выключает модуль." } },
        { "prev", { "Loads the previous preset.", "Загружает предыдущий пресет." } },
        { "next", { "Loads the next preset.", "Загружает следующий пресет." } },
        { "pname", { "Opens the preset list.", "Открывает список пресетов." } },
        { "save", { "Saves the settings as a user preset.", "Сохраняет настройки как пользовательский пресет." } },
        { "rename", { "Gives a new name to the user preset.", "Даёт пользовательскому пресету новое имя." } },
        { "del", { "Deletes the user preset. The file goes to the Recycle Bin.", "Удаляет пользовательский пресет. Файл попадает в Корзину." } },
        { "folder", { "Opens the user preset folder.", "Открывает папку пользовательских пресетов." } },
        { "ru", { "Hint language: Russian.", "Язык подсказок: русский." } },
        { "en", { "Hint language: English.", "Язык подсказок: английский." } },
        { "upd", { "Checks for a new version on GitHub.", "Проверяет новую версию на GitHub." } },
        { "z100", { "Window scale 100 %.", "Масштаб окна 100 %." } },
        { "z125", { "Window scale 125 %.", "Масштаб окна 125 %." } },
        { "latency", { "Latency that the plugin reports to the DAW. 0 = no latency.", "Задержка, которую плагин сообщает DAW. 0 = задержки нет." } },
    };
    return t;
}

const juce::Identifier kHintKey ("pgHintKey"), kHintLabel ("pgHintLabel");
} // namespace

juce::String hintText (const juce::String& key, Lang lang)
{
    const auto& t = table();
    auto it = t.find (key);
    if (it == t.end())
        return {};
    return juce::String::fromUTF8 (lang == Lang::Ru ? it->second.ru : it->second.en);
}

juce::String idleHint (Lang lang)
{
    return lang == Lang::Ru ? juce::String::fromUTF8 ("Наведите курсор на модуль или ручку, чтобы увидеть описание.")
                            : juce::String ("Move the mouse over a module or a knob to see the description.");
}

juce::String hintKeyForParam (const juce::String& paramId)
{
    if (paramId.startsWith ("eq_b") && paramId.length() > 5 && juce::CharacterFunctions::isDigit (paramId[4]))
        return "eq_b" + paramId.substring (5); // eq_b1_freq -> eq_b_freq
    return paramId;
}

void setHint (juce::Component& c, const juce::String& key, const juce::String& label)
{
    c.getProperties().set (kHintKey, key);
    c.getProperties().set (kHintLabel, label);
}

bool getFixedHint (const juce::Component& c, juce::String& key, juce::String& label)
{
    if (! c.getProperties().contains (kHintKey))
        return false;
    key = c.getProperties()[kHintKey].toString();
    label = c.getProperties()[kHintLabel].toString();
    return true;
}
} // namespace pg::ui
