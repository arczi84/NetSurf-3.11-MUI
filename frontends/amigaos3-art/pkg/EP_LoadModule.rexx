/* Start EaglePlayer if not allready started. Optional: load a given file */

options
parse arg EaglePlayer filename

filename=strip(filename)

if show('PORTS','rexx_EP')  then do
   address 'rexx_EP' Loadmodule filename
   exit
end
else do
  address COMMAND 'Run  ' EaglePlayer Loadmodule " " filename
end

