final: prev: {
  docs = prev.callPackage ./book.nix { };

  devShell = final.docs;
}
