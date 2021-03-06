<?php
$gLines = file("syscalls.lst");

$lSyscalls = array();
$i = 0;
foreach($gLines as $line)
{
	$line = trim($line);
	if(empty($line))	continue;
	
	if( intVal($line) != 0)
		$i = $line;
	else
		$lSyscalls[$i++] = explode("\t", $line, 3);
}
$lMax = $i;

$lAsmInc = "; Acess2
; System Calls List
; 

";
$lHeader  = "/*
 * AcessOS Microkernel Version
 * syscalls.h
 */
#ifndef _SYSCALLS_H
#define _SYSCALLS_H

";
$i = 0;
foreach($lSyscalls as $num=>$call)
{
	if($i != $num)	{
		$lHeader .= "\n";
		$lAsmInc .= "\n";
	}
	
	$lHeader .= "#define {$call[0]}\t{$num}";
	$lHeader .= "\t// {$num} - {$call[1]}\n";
	
	$lAsmInc .= "%define {$call[0]}\t{$num}\t; {$call[1]}\n";
	
	
	if($i != $num)
		$i = $num+1;
	else
		$i ++;
}
$lHeader .= "#define NUM_SYSCALLS\t$i\n";
$lHeader .= "#define SYS_DEBUG\t0x100	// 0x100 - Print a debug string\n";
$lHeader .= "\n";
$lHeader .= "#ifdef __GNUC__\n";
$lHeader .= "static const char *cSYSCALL_NAMES[] = {\n\t";

$j = 0;
for($i=0;$i<$lMax;$i++)
{
	if(!isset($lSyscalls[$i]))
		$lHeader .= "\"\",";
	else
		$lHeader .= "\"".$lSyscalls[$i][0]."\",";
	$j ++;
	if($j == 6) {
		$lHeader .= "\n\t";
		$j = 0;
	}
}
$lHeader .= "\"\"\n};\n"
$lHeader .= "#endif\n";
$lHeader .= "#endif\n";

$fp = fopen("include/syscalls.h", "w");	fwrite($fp, $lHeader);	fclose($fp);
$fp = fopen("include/syscalls.inc.asm", "w");	fwrite($fp, $lAsmInc);	fclose($fp);

?>
