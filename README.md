peryl
=====

Run python scripts within the Erlang VM. Use at your own peryl and prefer the
alternative of just launching under a port.


Written using NIF support.

Build
-----

    $ rebar3 compile

Usage
-----

    peryl:run("myscript.py").
    
    
Caveats
-------

Work in progress, with most of that work remaining,  Written to see if it was
possible to schedule python scripts as erlang processes and pass results from
one script to the other (and it's not even that far yet).


