# several utility procs for usage in other scripts
# don't export anything, just use it from the namespace
#
# these procs are not specific to anything special,
# they could be useful in any script.
#
# Born to prevent duplication between scripts for common stuff.

namespace eval utils {

proc get_machine_display_name { { machineid "" } } {
	if {$machineid eq ""} {
		set machineid [machine]
	}
	set config_name [${machineid}::machine_info config_name]
	array set names [openmsx_info machines $config_name]
	return [format "%s %s" $names(manufacturer) $names(code)]
}

proc get_machine_time { { machineid "" } } {
	if {$machineid eq ""} {
		set machineid [machine]
	}
	set mtime [${machineid}::machine_info time]
	return [format "%02d:%02d:%02d" [expr int($mtime / 3600)] [expr int($mtime / 60) % 60] [expr int($mtime) % 60]]
}

} ;# namespace utils