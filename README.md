# FUSE_RAID_1

./net_raid_client config_file უშვებს fuse ს როგორც foreground პროცესს, შესაბამისად როდესაც კლიენტი ctr-c ამთავრებს მუშაობას, დამაუნთებული დირექტორია არ საჭირეობს umount ს.
მხოლოდ იმ შემთხევაში როცა მშობელი პროცესის output იქნება SOMETHING BAD HAPPENED საჭიროა დირექტორიის umount. 
საჭიროა , რომ ./net_raid_server ის თვის გადაცემული დირექტორიები უკვე არსებობდეს და მხოლოდ ჩართულ სერვერთან შეუძლია clientს შეერთების დამყარება.


იმპლემენტაციის დეტალები -> DESIGNDOC