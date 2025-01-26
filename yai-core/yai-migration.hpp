namespace yai::migration {

class Migration {
public:
  Migration(const char *migrate_q, const char *rollback_q,
            const char *fixtures_q, const char *drop_q);

  int Start(int argc, char *argv[]);

private:
  const char *migrate_q_, *rollback_q_, *fixtures_q_, *drop_q_;
};

} // namespace yai::migration
