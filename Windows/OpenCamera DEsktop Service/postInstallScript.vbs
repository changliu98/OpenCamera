Dim curdir, WshShell
curdir = Session.Property("CustomActionData")
Set WshShell = CreateObject ("WScript.Shell")
WshShell.Run("regsvr32 " & chr(34) & curdir & "EncomCamerax64.dll" & chr(34))
WshShell.Run("regsvr32 " & chr(34) & curdir & "EncomCamerax86.dll" & chr(34))