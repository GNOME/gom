using GLib;
using GObject;
using Gom;

namespace Test {
	public class Person: Gom.Resource {
		construct {
		}
	}

	static int main (string[] args) {
		var adapter = new Gom.AdapterSqlite();
		try {
			adapter.load_from_file("test-vala-gom.db");
			var person = new Person({
				adapter = adapter,
				is_new = true
			});
			stdout.printf("Is new? %d", person.is_new());
			person.save();
			adapter.close();
		} catch (Error err) {
			stderr.printf("%s", err.message);
		}
		return 0;
	}
}
