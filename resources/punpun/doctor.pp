// Reports the environment the IDE's toolchain resolves to. The IDE runs this
// after opening a project and parses the fixed marker below.
//
// Written in modern PunPun on purpose: this file ships inside an IDE for the
// language, so it is the first PunPun many people read. The migration dialect
// it used to be written in (`bring`, `launch:` ... `done`, bare `say x`) still
// compiles, but PPC answers every one of those with warning W2000.
import std.system

launch {
    say("PUNPUN_IDE_DOCTOR_V1");
    say(system_platform());
    say(system_current_dir());
    say(system_env_or("PATH", ""));
    say(system_env_or("PPC_RUNTIME", ""));
    say(system_env_or("PPC_STDLIB", ""));
    say(system_env_or("CC", ""));
    say(system_env_or("CXX", ""));
}
