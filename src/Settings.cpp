#include "Settings.h"

IdeSettings::IdeSettings(): s_("PunPun", "PunPun IDE") {}
QString IdeSettings::theme() const { return s_.value("appearance/theme", "Dark Modern").toString(); }
void IdeSettings::setTheme(const QString &v){ s_.setValue("appearance/theme",v); }
QString IdeSettings::fontFamily() const { return s_.value("editor/fontFamily", "JetBrains Mono").toString(); }
void IdeSettings::setFontFamily(const QString &v){ s_.setValue("editor/fontFamily",v); }
int IdeSettings::fontSize() const { return s_.value("editor/fontSize", 13).toInt(); }
void IdeSettings::setFontSize(int v){ s_.setValue("editor/fontSize",qBound(8,v,32)); }
int IdeSettings::tabWidth() const { return s_.value("editor/tabWidth", 4).toInt(); }
void IdeSettings::setTabWidth(int v){ s_.setValue("editor/tabWidth",qBound(2,v,8)); }
bool IdeSettings::wordWrap() const { return s_.value("editor/wordWrap", false).toBool(); }
void IdeSettings::setWordWrap(bool v){ s_.setValue("editor/wordWrap",v); }
bool IdeSettings::autoIndent() const { return s_.value("editor/autoIndent", true).toBool(); }
void IdeSettings::setAutoIndent(bool v){ s_.setValue("editor/autoIndent",v); }
bool IdeSettings::autoCloseBrackets() const { return s_.value("editor/autoCloseBrackets", true).toBool(); }
void IdeSettings::setAutoCloseBrackets(bool v){ s_.setValue("editor/autoCloseBrackets",v); }
bool IdeSettings::highlightCurrentLine() const { return s_.value("editor/highlightCurrentLine", true).toBool(); }
void IdeSettings::setHighlightCurrentLine(bool v){ s_.setValue("editor/highlightCurrentLine",v); }
bool IdeSettings::showWhitespace() const { return s_.value("editor/showWhitespace", false).toBool(); }
void IdeSettings::setShowWhitespace(bool v){ s_.setValue("editor/showWhitespace",v); }
bool IdeSettings::trimTrailingWhitespace() const { return s_.value("files/trimTrailingWhitespace", true).toBool(); }
void IdeSettings::setTrimTrailingWhitespace(bool v){ s_.setValue("files/trimTrailingWhitespace",v); }
bool IdeSettings::insertFinalNewline() const { return s_.value("files/insertFinalNewline", true).toBool(); }
void IdeSettings::setInsertFinalNewline(bool v){ s_.setValue("files/insertFinalNewline",v); }
int IdeSettings::terminalFontSize() const { return s_.value("terminal/fontSize", 12).toInt(); }
void IdeSettings::setTerminalFontSize(int v){ s_.setValue("terminal/fontSize",qBound(8,v,32)); }
bool IdeSettings::restoreLastProject() const { return s_.value("workspace/restoreLastProject", true).toBool(); }
void IdeSettings::setRestoreLastProject(bool v){ s_.setValue("workspace/restoreLastProject",v); }
bool IdeSettings::autoSave() const { return s_.value("files/autoSave", false).toBool(); }
void IdeSettings::setAutoSave(bool v){ s_.setValue("files/autoSave",v); }
bool IdeSettings::autoUpdatePunPun() const { return s_.value("punpun/autoUpdate", true).toBool(); }
void IdeSettings::setAutoUpdatePunPun(bool v){ s_.setValue("punpun/autoUpdate",v); }
int IdeSettings::updateIntervalMinutes() const { return s_.value("punpun/updateIntervalMinutes", 10).toInt(); }
void IdeSettings::setUpdateIntervalMinutes(int v){ s_.setValue("punpun/updateIntervalMinutes",qBound(5,v,1440)); }
QString IdeSettings::lastProject() const { return s_.value("workspace/lastProject", "").toString(); }
void IdeSettings::setLastProject(const QString &v){ s_.setValue("workspace/lastProject",v); }
QMap<QString,QString> IdeSettings::keybindings() const {
    QMap<QString,QString> out{{"file.open","Ctrl+O"},{"file.openFolder","Ctrl+K, Ctrl+O"},{"file.save","Ctrl+S"},{"file.saveAll","Ctrl+Shift+S"},
                             {"file.new","Ctrl+N"},{"run.run","F5"},{"run.debug","Ctrl+F5"},{"run.check","Ctrl+Shift+B"},
                             {"view.terminal","Ctrl+`"},{"view.assistant","Ctrl+Shift+A"},{"editor.complete","Ctrl+Space"},{"editor.find","Ctrl+F"},{"editor.format","Shift+Alt+F"},{"editor.comment","Ctrl+/"},{"editor.indent","Ctrl+]"},{"editor.dedent","Ctrl+["},{"editor.duplicate","Ctrl+Shift+D"},{"editor.moveUp","Alt+Up"},{"editor.moveDown","Alt+Down"}};
    s_.beginGroup("keybindings");
    for (const auto &key: s_.childKeys()) out[key]=s_.value(key).toString();
    s_.endGroup(); return out;
}
void IdeSettings::setKeybinding(const QString &id,const QString &seq){ s_.setValue("keybindings/"+id,seq); }
