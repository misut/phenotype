export module phenotype.macos;

extern "C" int phenotype_macos_run_files_application(int argc, char *argv[]);

export namespace phenotype::macos {

inline int RunFilesApplication(int argc, char *argv[]) {
  return phenotype_macos_run_files_application(argc, argv);
}

} // namespace phenotype::macos
