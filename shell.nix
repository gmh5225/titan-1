{ pkgs ? import <unstable> {} }:
let
  stdenv = pkgs.llvmPackages_15.stdenv;

  triton = stdenv.mkDerivation rec {
    version = "dev-v1.0";
    name = "triton-${version}";

    src = pkgs.fetchFromGitHub {
      owner = "JonathanSalwan";
      repo = "Triton";
      rev = "6095a21c332caa3435e5ce9a88f544f2b9c3be5b";
      sha256 = "sha256-beexdXd48i3OHPOPOOJ59go1+UH1fqL7JRjwl14bKRQ=";
    };

    cmakeFlags = [
      "-DBOOST_INTERFACE=OFF"
      "-DBUILD_EXAMPLES=OFF"
      "-DENABLE_TEST=OFF"
      "-DPYTHON_BINDINGS=OFF"
      "-DLLVM_INTERFACE=ON"
    ];

    nativeBuildInputs = [
      pkgs.cmake
    ];

    buildInputs = [
      pkgs.capstone
      pkgs.llvm_15
      pkgs.z3
    ];
  };

in rec {
  titan = stdenv.mkDerivation {
    name = "titan";

    nativeBuildInputs = [
      pkgs.cmake
      pkgs.ninja
      pkgs.clang_15
      pkgs.graphviz
    ];

    buildInputs = [
      pkgs.range-v3
      pkgs.fmt
      pkgs.llvm_15
      triton
    ];
  };
}
