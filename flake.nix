#    libxclip -- If xclip / xsel was a C library
#    Copyright (C) 2024  Emma Bastås
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.



{
  description = "";

  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        {
          devShells.default = pkgs.mkShell {
            buildInputs = with pkgs; [
              gcc
              pkg-config
              cpplint
              valgrind
              xorg.libX11
              libGL.dev
              harfbuzz
              (pkgs.python3.withPackages (python-pkgs: with python-pkgs; [
                cogapp
                python-lsp-server
                rope # autocomplete
                pyflakes # syntax checking
                pycodestyle # style linting
                pylsp-mypy # type checking
                future # solves https://github.com/tomv564/pyls-mypy/issues/37
              ]))
            ];
          };
        }
      );
}
